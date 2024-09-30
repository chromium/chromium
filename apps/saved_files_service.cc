// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/saved_files_service.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>

#include "apps/saved_files_service_factory.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/file_system/saved_file_entry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

namespace apps {

using extensions::APIPermission;
using extensions::Extension;
using extensions::ExtensionHost;
using extensions::ExtensionPrefs;
using extensions::SavedFileEntry;

namespace {

// Preference keys

// The file entries that the app has permission to access.
const char kFileEntries[] = "file_entries";

// The path to a file entry that the app had permission to access.
const char kFileEntryPath[] = "path";

// Whether or not the the entry refers to a directory.
const char kFileEntryIsDirectory[] = "is_directory";

// The sequence number in the LRU of the file entry.
const char kFileEntrySequenceNumber[] = "sequence_number";

const size_t kMaxSavedFileEntries = 500;
const int kMaxSequenceNumber = INT32_MAX;

// These might be different to the constant values in tests.
size_t g_max_saved_file_entries = kMaxSavedFileEntries;
int g_max_sequence_number = kMaxSequenceNumber;

// Persists a SavedFileEntry in ExtensionPrefs.
void AddSavedFileEntry(ExtensionPrefs* prefs,
                       const extensions::ExtensionId& extension_id,
                       const SavedFileEntry& file_entry) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      prefs, extension_id, kFileEntries);
  auto file_entries = update.Create();
  DCHECK(
      !file_entries->GetDictionaryWithoutPathExpansion(file_entry.id, nullptr));

  base::Value::Dict file_entry_dict;
  file_entry_dict.Set(kFileEntryPath, base::FilePathToValue(file_entry.path));
  file_entry_dict.Set(kFileEntryIsDirectory, file_entry.is_directory);
  file_entry_dict.Set(kFileEntrySequenceNumber, file_entry.sequence_number);
  file_entries->SetDictionaryWithoutPathExpansion(file_entry.id,
                                                  std::move(file_entry_dict));
}

// Updates the sequence_number of a SavedFileEntry persisted in ExtensionPrefs.
void UpdateSavedFileEntry(ExtensionPrefs* prefs,
                          const extensions::ExtensionId& extension_id,
                          const SavedFileEntry& file_entry) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      prefs, extension_id, kFileEntries);
  auto file_entries = update.Get();
  DCHECK(file_entries);
  std::unique_ptr<prefs::DictionaryValueUpdate> file_entry_dict;
  file_entries->GetDictionaryWithoutPathExpansion(file_entry.id,
                                                  &file_entry_dict);
  DCHECK(file_entry_dict);
  file_entry_dict->SetInteger(kFileEntrySequenceNumber,
                              file_entry.sequence_number);
}

// Removes a SavedFileEntry from ExtensionPrefs.
void RemoveSavedFileEntry(ExtensionPrefs* prefs,
                          const extensions::ExtensionId& extension_id,
                          const std::string& file_entry_id) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      prefs, extension_id, kFileEntries);
  auto file_entries = update.Create();
  file_entries->RemoveWithoutPathExpansion(file_entry_id, NULL);
}

// Clears all SavedFileEntry for the app from ExtensionPrefs.
void ClearSavedFileEntries(ExtensionPrefs* prefs,
                           const extensions::ExtensionId& extension_id) {
  prefs->UpdateExtensionPref(extension_id, kFileEntries, std::nullopt);
}

// Returns all SavedFileEntries for the app.
std::vector<SavedFileEntry> GetSavedFileEntries(
    ExtensionPrefs* prefs,
    const extensions::ExtensionId& extension_id) {
  std::vector<SavedFileEntry> result;

  const auto* dict = prefs->ReadPrefAsDict(extension_id, kFileEntries);
  if (!dict) {
    return result;
  }

  for (const auto item : *dict) {
    const auto* file_entry = item.second.GetIfDict();
    if (!file_entry)
      continue;

    const base::Value* path_value = file_entry->Find(kFileEntryPath);
    if (!path_value)
      continue;
    std::optional<base::FilePath> file_path =
        base::ValueToFilePath(*path_value);
    if (!file_path)
      continue;
    bool is_directory =
        file_entry->FindBool(kFileEntryIsDirectory).value_or(false);
    const std::optional<int> sequence_number =
        file_entry->FindInt(kFileEntrySequenceNumber);
    if (!sequence_number || sequence_number.value() == 0)
      continue;
    result.emplace_back(item.first, *file_path, is_directory,
                        sequence_number.value());
  }
  return result;
}

}  // namespace

class SavedFilesService::SavedFiles {
 public:
  SavedFiles(content::BrowserContext* context,
             const extensions::ExtensionId& extension_id);
  SavedFiles(const SavedFiles&) = delete;
  SavedFiles& operator=(const SavedFiles&) = delete;
  ~SavedFiles();

  void RegisterFileEntry(const std::string& id,
                         const base::FilePath& file_path,
                         bool is_directory);
  void EnqueueFileEntry(const std::string& id);
  bool IsRegistered(const std::string& id) const;
  const SavedFileEntry* GetFileEntry(const std::string& id) const;
  std::vector<SavedFileEntry> GetAllFileEntries() const;

 private:
  // Compacts sequence numbers if the largest sequence number is
  // g_max_sequence_number. Outside of testing, it is set to kint32max, so this
  // will almost never do any real work.
  void MaybeCompactSequenceNumbers();

  void LoadSavedFileEntriesFromPreferences();

  raw_ptr<content::BrowserContext> context_;
  const extensions::ExtensionId extension_id_;

  // Contains all file entries that have been registered, keyed by ID.
  std::unordered_map<std::string, std::unique_ptr<SavedFileEntry>>
      registered_file_entries_;

  // The queue of file entries that have been retained, keyed by
  // sequence_number. Values are a subset of values in registered_file_entries_.
  // This should be kept in sync with file entries stored in extension prefs.
  std::map<int, raw_ptr<SavedFileEntry, CtnExperimental>> saved_file_lru_;
};

// static
SavedFilesService* SavedFilesService::Get(content::BrowserContext* context) {
  return SavedFilesServiceFactory::GetForBrowserContext(context);
}

SavedFilesService::SavedFilesService(content::BrowserContext* context)
    : context_(context) {
  extension_host_registry_observation_.Observe(
      extensions::ExtensionHostRegistry::Get(context_));
}

SavedFilesService::~SavedFilesService() = default;

void SavedFilesService::OnExtensionHostDestroyed(
    content::BrowserContext* browser_context,
    extensions::ExtensionHost* host) {
  const Extension* extension = host->extension();
  if (extension) {
    ClearQueueIfNoRetainPermission(extension);
    Clear(extension->id());
  }
}

void SavedFilesService::RegisterFileEntry(
    const extensions::ExtensionId& extension_id,
    const std::string& id,
    const base::FilePath& file_path,
    bool is_directory) {
  GetOrInsert(extension_id)->RegisterFileEntry(id, file_path, is_directory);
}

void SavedFilesService::EnqueueFileEntry(
    const extensions::ExtensionId& extension_id,
    const std::string& id) {
  GetOrInsert(extension_id)->EnqueueFileEntry(id);
}

std::vector<SavedFileEntry> SavedFilesService::GetAllFileEntries(
    const extensions::ExtensionId& extension_id) {
  SavedFiles* saved_files = Get(extension_id);
  if (saved_files)
    return saved_files->GetAllFileEntries();
  return GetSavedFileEntries(ExtensionPrefs::Get(context_), extension_id);
}

bool SavedFilesService::IsRegistered(
    const extensions::ExtensionId& extension_id,
    const std::string& id) {
  return GetOrInsert(extension_id)->IsRegistered(id);
}

const SavedFileEntry* SavedFilesService::GetFileEntry(
    const extensions::ExtensionId& extension_id,
    const std::string& id) {
  return GetOrInsert(extension_id)->GetFileEntry(id);
}

void SavedFilesService::ClearQueueIfNoRetainPermission(
    const Extension* extension) {
  if (!extension->permissions_data()->active_permissions().HasAPIPermission(
          extensions::mojom::APIPermissionID::kFileSystemRetainEntries)) {
    ClearQueue(extension);
  }
}

void SavedFilesService::ClearQueue(const extensions::Extension* extension) {
  ClearSavedFileEntries(ExtensionPrefs::Get(context_), extension->id());
  Clear(extension->id());
}

void SavedFilesService::OnApplicationTerminating() {
  // Stop listening to ExtensionHost shutdown as all extension hosts will be
  // destroyed as a result of shutdown.
  extension_host_registry_observation_.Reset();
}

SavedFilesService::SavedFiles* SavedFilesService::Get(
    const extensions::ExtensionId& extension_id) const {
  auto it = extension_id_to_saved_files_.find(extension_id);
  if (it != extension_id_to_saved_files_.end())
    return it->second.get();

  return NULL;
}

SavedFilesService::SavedFiles* SavedFilesService::GetOrInsert(
    const extensions::ExtensionId& extension_id) {
  SavedFiles* saved_files = Get(extension_id);
  if (saved_files)
    return saved_files;

  std::unique_ptr<SavedFiles> scoped_saved_files(
      new SavedFiles(context_, extension_id));
  saved_files = scoped_saved_files.get();
  extension_id_to_saved_files_.insert(
      std::make_pair(extension_id, std::move(scoped_saved_files)));
  return saved_files;
}

void SavedFilesService::Clear(const extensions::ExtensionId& extension_id) {
  extension_id_to_saved_files_.erase(extension_id);
}

SavedFilesService::SavedFiles::SavedFiles(
    content::BrowserContext* context,
    const extensions::ExtensionId& extension_id)
    : context_(context), extension_id_(extension_id) {
  LoadSavedFileEntriesFromPreferences();
}

SavedFilesService::SavedFiles::~SavedFiles() = default;

void SavedFilesService::SavedFiles::RegisterFileEntry(
    const std::string& id,
    const base::FilePath& file_path,
    bool is_directory) {
  auto it = registered_file_entries_.find(id);
  if (it != registered_file_entries_.end())
    return;

  registered_file_entries_[id] =
      std::make_unique<SavedFileEntry>(id, file_path, is_directory, 0);
}

void SavedFilesService::SavedFiles::EnqueueFileEntry(const std::string& id) {
  auto id_it = registered_file_entries_.find(id);
  CHECK(id_it != registered_file_entries_.end(), base::NotFatalUntil::M130);

  SavedFileEntry* file_entry = id_it->second.get();
  int old_sequence_number = file_entry->sequence_number;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // crbug.com/983844 Convert path from legacy Download/ to MyFiles/Downloads/
  // so entries saved before MyFiles don't fail. TODO(lucmult): Remove this
  // after M-83.
  const auto legacy_downloads = context_->GetPath().AppendASCII("Downloads");
  auto to_myfiles =
      context_->GetPath().AppendASCII("MyFiles").AppendASCII("Downloads");
  if (legacy_downloads.AppendRelativePath(file_entry->path, &to_myfiles))
    file_entry->path = to_myfiles;
#endif

  if (!saved_file_lru_.empty()) {
    // Get the sequence number after the last file entry in the LRU.
    std::map<int, raw_ptr<SavedFileEntry, CtnExperimental>>::reverse_iterator
        it = saved_file_lru_.rbegin();
    if (it->second == file_entry)
      return;

    file_entry->sequence_number = it->first + 1;
  } else {
    // The first sequence number is 1, as 0 means the entry is not in the LRU.
    file_entry->sequence_number = 1;
  }
  saved_file_lru_.insert(
      std::make_pair(file_entry->sequence_number, file_entry));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context_);
  if (old_sequence_number) {
    saved_file_lru_.erase(old_sequence_number);
    UpdateSavedFileEntry(prefs, extension_id_, *file_entry);
  } else {
    AddSavedFileEntry(prefs, extension_id_, *file_entry);
    if (saved_file_lru_.size() > g_max_saved_file_entries) {
      std::map<int, raw_ptr<SavedFileEntry, CtnExperimental>>::iterator it =
          saved_file_lru_.begin();
      it->second->sequence_number = 0;
      RemoveSavedFileEntry(prefs, extension_id_, it->second->id);
      saved_file_lru_.erase(it);
    }
  }
  MaybeCompactSequenceNumbers();
}

bool SavedFilesService::SavedFiles::IsRegistered(const std::string& id) const {
  auto it = registered_file_entries_.find(id);
  return it != registered_file_entries_.end();
}

const SavedFileEntry* SavedFilesService::SavedFiles::GetFileEntry(
    const std::string& id) const {
  auto it = registered_file_entries_.find(id);
  if (it == registered_file_entries_.end())
    return NULL;

  return it->second.get();
}

std::vector<SavedFileEntry> SavedFilesService::SavedFiles::GetAllFileEntries()
    const {
  std::vector<SavedFileEntry> result;
  for (auto it = registered_file_entries_.begin();
       it != registered_file_entries_.end(); ++it) {
    result.push_back(*it->second.get());
  }
  return result;
}

void SavedFilesService::SavedFiles::MaybeCompactSequenceNumbers() {
  DCHECK_GE(g_max_sequence_number, 0);
  DCHECK_GE(static_cast<size_t>(g_max_sequence_number),
            g_max_saved_file_entries);
  std::map<int, raw_ptr<SavedFileEntry, CtnExperimental>>::reverse_iterator
      last_it = saved_file_lru_.rbegin();
  if (last_it == saved_file_lru_.rend())
    return;

  // Only compact sequence numbers if the last entry's sequence number is the
  // maximum value.  This should almost never be the case.
  if (last_it->first < g_max_sequence_number)
    return;

  int sequence_number = 0;
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context_);
  for (std::map<int, raw_ptr<SavedFileEntry, CtnExperimental>>::iterator it =
           saved_file_lru_.begin();
       it != saved_file_lru_.end(); ++it) {
    sequence_number++;
    if (it->second->sequence_number == sequence_number)
      continue;

    SavedFileEntry* file_entry = it->second;
    file_entry->sequence_number = sequence_number;
    UpdateSavedFileEntry(prefs, extension_id_, *file_entry);
    saved_file_lru_.erase(it++);
    // Provide the following element as an insert hint. While optimized
    // insertion time with the following element as a hint is only supported by
    // the spec in C++11, the implementations do support this.
    it = saved_file_lru_.insert(
        it, std::make_pair(file_entry->sequence_number, file_entry));
  }
}

void SavedFilesService::SavedFiles::LoadSavedFileEntriesFromPreferences() {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context_);
  std::vector<SavedFileEntry> saved_entries =
      GetSavedFileEntries(prefs, extension_id_);
  for (std::vector<SavedFileEntry>::iterator it = saved_entries.begin();
       it != saved_entries.end();
       ++it) {
    std::unique_ptr<SavedFileEntry> file_entry(new SavedFileEntry(*it));
    const std::string& id = file_entry->id;
    saved_file_lru_.insert(
        std::make_pair(file_entry->sequence_number, file_entry.get()));
    registered_file_entries_[id] = std::move(file_entry);
  }
}

// static
void SavedFilesService::SetMaxSequenceNumberForTest(int max_value) {
  g_max_sequence_number = max_value;
}

// static
void SavedFilesService::ClearMaxSequenceNumberForTest() {
  g_max_sequence_number = kMaxSequenceNumber;
}

// static
void SavedFilesService::SetLruSizeForTest(int size) {
  g_max_saved_file_entries = size;
}

// static
void SavedFilesService::ClearLruSizeForTest() {
  g_max_saved_file_entries = kMaxSavedFileEntries;
}

}  // namespace apps
