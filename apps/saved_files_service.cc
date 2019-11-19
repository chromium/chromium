// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/saved_files_service.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <unordered_map>
#include <utility>

#include "apps/saved_files_service_factory.h"
#include "base/value_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/api/file_system/saved_file_entry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
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
                       const std::string& extension_id,
                       const SavedFileEntry& file_entry) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      prefs, extension_id, kFileEntries);
  auto file_entries = update.Create();
  DCHECK(!file_entries->GetDictionaryWithoutPathExpansion(file_entry.id, NULL));

  std::unique_ptr<base::DictionaryValue> file_entry_dict =
      std::make_unique<base::DictionaryValue>();
  file_entry_dict->SetKey(kFileEntryPath, CreateFilePathValue(file_entry.path));
  file_entry_dict->SetBoolean(kFileEntryIsDirectory, file_entry.is_directory);
  file_entry_dict->SetInteger(kFileEntrySequenceNumber,
                              file_entry.sequence_number);
  file_entries->SetWithoutPathExpansion(file_entry.id,
                                        std::move(file_entry_dict));
}

// Updates the sequence_number of a SavedFileEntry persisted in ExtensionPrefs.
void UpdateSavedFileEntry(ExtensionPrefs* prefs,
                          const std::string& extension_id,
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
                          const std::string& extension_id,
                          const std::string& file_entry_id) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      prefs, extension_id, kFileEntries);
  auto file_entries = update.Create();
  file_entries->RemoveWithoutPathExpansion(file_entry_id, NULL);
}

// Clears all SavedFileEntry for the app from ExtensionPrefs.
void ClearSavedFileEntries(ExtensionPrefs* prefs,
                           const std::string& extension_id) {
  prefs->UpdateExtensionPref(extension_id, kFileEntries, nullptr);
}

// Returns all SavedFileEntries for the app.
std::vector<SavedFileEntry> GetSavedFileEntries(
    ExtensionPrefs* prefs,
    const std::string& extension_id) {
  std::vector<SavedFileEntry> result;
  const base::DictionaryValue* file_entries = NULL;
  if (!prefs->ReadPrefAsDictionary(extension_id, kFileEntries, &file_entries))
    return result;

  for (base::DictionaryValue::Iterator it(*file_entries); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* file_entry = NULL;
    if (!it.value().GetAsDictionary(&file_entry))
      continue;
    const base::Value* path_value;
    if (!file_entry->Get(kFileEntryPath, &path_value))
      continue;
    base::FilePath file_path;
    if (!GetValueAsFilePath(*path_value, &file_path))
      continue;
    bool is_directory = false;
    file_entry->GetBoolean(kFileEntryIsDirectory, &is_directory);
    int sequence_number = 0;
    if (!file_entry->GetInteger(kFileEntrySequenceNumber, &sequence_number))
      continue;
    if (!sequence_number)
      continue;
    result.push_back(
        SavedFileEntry(it.key(), file_path, is_directory, sequence_number));
  }
  return result;
}

}  // namespace

class SavedFilesService::SavedFiles {
 public:
  SavedFiles(content::BrowserContext* context, const std::string& extension_id);
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

  content::BrowserContext* context_;
  const std::string extension_id_;

  // Contains all file entries that have been registered, keyed by ID.
  std::unordered_map<std::string, std::unique_ptr<SavedFileEntry>>
      registered_file_entries_;

  // The queue of file entries that have been retained, keyed by
  // sequence_number. Values are a subset of values in registered_file_entries_.
  // This should be kept in sync with file entries stored in extension prefs.
  std::map<int, SavedFileEntry*> saved_file_lru_;

  DISALLOW_COPY_AND_ASSIGN(SavedFiles);
};

// static
SavedFilesService* SavedFilesService::Get(content::BrowserContext* context) {
  return SavedFilesServiceFactory::GetForBrowserContext(context);
}

SavedFilesService::SavedFilesService(content::BrowserContext* context)
    : context_(context) {
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::NotificationService::AllSources());
}

SavedFilesService::~SavedFilesService() = default;

void SavedFilesService::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED, type);
  ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
  const Extension* extension = host->extension();
  if (extension) {
    ClearQueueIfNoRetainPermission(extension);
    Clear(extension->id());
  }
}

void SavedFilesService::RegisterFileEntry(const std::string& extension_id,
                                          const std::string& id,
                                          const base::FilePath& file_path,
                                          bool is_directory) {
  GetOrInsert(extension_id)->RegisterFileEntry(id, file_path, is_directory);
}

void SavedFilesService::EnqueueFileEntry(const std::string& extension_id,
                                         const std::string& id) {
  GetOrInsert(extension_id)->EnqueueFileEntry(id);
}

std::vector<SavedFileEntry> SavedFilesService::GetAllFileEntries(
    const std::string& extension_id) {
  SavedFiles* saved_files = Get(extension_id);
  if (saved_files)
    return saved_files->GetAllFileEntries();
  return GetSavedFileEntries(ExtensionPrefs::Get(context_), extension_id);
}

bool SavedFilesService::IsRegistered(const std::string& extension_id,
                                     const std::string& id) {
  return GetOrInsert(extension_id)->IsRegistered(id);
}

const SavedFileEntry* SavedFilesService::GetFileEntry(
    const std::string& extension_id,
    const std::string& id) {
  return GetOrInsert(extension_id)->GetFileEntry(id);
}

void SavedFilesService::ClearQueueIfNoRetainPermission(
    const Extension* extension) {
  if (!extension->permissions_data()->active_permissions().HasAPIPermission(
          APIPermission::kFileSystemRetainEntries)) {
    ClearQueue(extension);
  }
}

void SavedFilesService::ClearQueue(const extensions::Extension* extension) {
  ClearSavedFileEntries(ExtensionPrefs::Get(context_), extension->id());
  Clear(extension->id());
}

void SavedFilesService::OnApplicationTerminating() {
  // Stop listening to NOTIFICATION_EXTENSION_HOST_DESTROYED in particular
  // as all extension hosts will be destroyed as a result of shutdown.
  registrar_.RemoveAll();
}

SavedFilesService::SavedFiles* SavedFilesService::Get(
    const std::string& extension_id) const {
  auto it = extension_id_to_saved_files_.find(extension_id);
  if (it != extension_id_to_saved_files_.end())
    return it->second.get();

  return NULL;
}

SavedFilesService::SavedFiles* SavedFilesService::GetOrInsert(
    const std::string& extension_id) {
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

void SavedFilesService::Clear(const std::string& extension_id) {
  extension_id_to_saved_files_.erase(extension_id);
}

SavedFilesService::SavedFiles::SavedFiles(content::BrowserContext* context,
                                          const std::string& extension_id)
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
  auto it = registered_file_entries_.find(id);
  DCHECK(it != registered_file_entries_.end());

  SavedFileEntry* file_entry = it->second.get();
  int old_sequence_number = file_entry->sequence_number;

#if defined(OS_CHROMEOS)
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
    std::map<int, SavedFileEntry*>::reverse_iterator it =
        saved_file_lru_.rbegin();
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
      std::map<int, SavedFileEntry*>::iterator it = saved_file_lru_.begin();
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
  std::map<int, SavedFileEntry*>::reverse_iterator it =
      saved_file_lru_.rbegin();
  if (it == saved_file_lru_.rend())
    return;

  // Only compact sequence numbers if the last entry's sequence number is the
  // maximum value.  This should almost never be the case.
  if (it->first < g_max_sequence_number)
    return;

  int sequence_number = 0;
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context_);
  for (std::map<int, SavedFileEntry*>::iterator it = saved_file_lru_.begin();
       it != saved_file_lru_.end();
       ++it) {
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
