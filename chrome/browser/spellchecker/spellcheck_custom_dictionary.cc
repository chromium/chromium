// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/hash/md5.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/common/chrome_constants.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/protocol/sync.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Filename extension for backup dictionary file.
const base::FilePath::CharType BACKUP_EXTENSION[] = FILE_PATH_LITERAL("backup");

// Prefix for the checksum in the dictionary file.
const char CHECKSUM_PREFIX[] = "checksum_v1 = ";

// The status of the checksum in a custom spellcheck dictionary.
enum ChecksumStatus {
  VALID_CHECKSUM,
  INVALID_CHECKSUM,
};

// The result of a dictionary sanitation. Can be used as a bitmap.
enum ChangeSanitationResult {
  // The change is valid and can be applied as-is.
  VALID_CHANGE = 0,

  // The change contained words to be added that are not valid.
  DETECTED_INVALID_WORDS = 1,

  // The change contained words to be added that are already in the dictionary.
  DETECTED_DUPLICATE_WORDS = 2,

  // The change contained words to be removed that are not in the dictionary.
  DETECTED_MISSING_WORDS = 4,
};

// Loads the file at |file_path| into the |words| container. If the file has a
// valid checksum, then returns ChecksumStatus::VALID. If the file has an
// invalid checksum, then returns ChecksumStatus::INVALID and clears |words|.
ChecksumStatus LoadFile(const base::FilePath& file_path,
                        std::set<std::string>* words) {
  DCHECK(words);
  words->clear();
  std::string contents;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    base::ReadFileToString(file_path, &contents);
  }
  size_t pos = contents.rfind(CHECKSUM_PREFIX);
  if (pos != std::string::npos) {
    std::string checksum = contents.substr(pos + strlen(CHECKSUM_PREFIX));
    contents = contents.substr(0, pos);
    if (checksum != base::MD5String(contents))
      return INVALID_CHECKSUM;
  }

  std::vector<std::string> word_list = base::SplitString(
      base::TrimWhitespaceASCII(contents, base::TRIM_ALL), "\n",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  words->insert(word_list.begin(), word_list.end());
  return VALID_CHECKSUM;
}

// Returns true for valid custom dictionary words.
bool IsValidWord(const std::string& word) {
  std::string tmp;
  return !word.empty() &&
         word.size() <= spellcheck::kMaxCustomDictionaryWordBytes &&
         base::IsStringUTF8(word) &&
         base::TRIM_NONE ==
             base::TrimWhitespaceASCII(word, base::TRIM_ALL, &tmp);
}

// Removes duplicate and invalid words from |to_add| word list. Looks for
// duplicates in both |to_add| and |existing| word lists. Returns a bitmap of
// |ChangeSanitationResult| values.
int SanitizeWordsToAdd(const std::set<std::string>& existing,
                       std::set<std::string>* to_add) {
  DCHECK(to_add);
  // Do not add duplicate words.
  std::vector<std::string> new_words =
      base::STLSetDifference<std::vector<std::string>>(*to_add, existing);
  int result = VALID_CHANGE;
  if (to_add->size() != new_words.size())
    result |= DETECTED_DUPLICATE_WORDS;
  // Do not add invalid words.
  std::set<std::string> valid_new_words;
  for (const auto& word : new_words) {
    if (IsValidWord(word))
      valid_new_words.insert(valid_new_words.end(), word);
  }
  if (valid_new_words.size() != new_words.size())
    result |= DETECTED_INVALID_WORDS;
  // Save the sanitized words to be added.
  std::swap(*to_add, valid_new_words);
  return result;
}

// Loads and returns the custom spellcheck dictionary from |path|. Must be
// called on the file thread.
std::unique_ptr<SpellcheckCustomDictionary::LoadFileResult>
LoadDictionaryFileReliably(const base::FilePath& path) {
  // Load the contents and verify the checksum.
  std::unique_ptr<SpellcheckCustomDictionary::LoadFileResult> result(
      new SpellcheckCustomDictionary::LoadFileResult);
  if (LoadFile(path, &result->words) == VALID_CHECKSUM) {
    result->is_valid_file =
        VALID_CHANGE ==
        SanitizeWordsToAdd(std::set<std::string>(), &result->words);
    return result;
  }
  // Checksum is not valid. See if there's a backup.
  base::FilePath backup = path.AddExtension(BACKUP_EXTENSION);
  if (base::PathExists(backup))
    LoadFile(backup, &result->words);
  SanitizeWordsToAdd(std::set<std::string>(), &result->words);
  return result;
}

// Backs up the original dictionary, saves |custom_words| and its checksum into
// the custom spellcheck dictionary at |path|.
void SaveDictionaryFileReliably(const base::FilePath& path,
                                const std::set<std::string>& custom_words) {
  std::stringstream content;
  for (const std::string& word : custom_words)
    content << word << '\n';

  std::string checksum = base::MD5String(content.str());
  content << CHECKSUM_PREFIX << checksum;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    base::CopyFile(path, path.AddExtension(BACKUP_EXTENSION));
    base::ImportantFileWriter::WriteFileAtomically(path, content.str());
  }
}

void SavePassedWordsToDictionaryFileReliably(
    const base::FilePath& path,
    std::unique_ptr<SpellcheckCustomDictionary::LoadFileResult>
        load_file_result) {
  DCHECK(load_file_result);
  SaveDictionaryFileReliably(path, load_file_result->words);
}

// Removes word from |to_remove| that are missing from |existing| word list and
// sorts |to_remove|. Returns a bitmap of |ChangeSanitationResult| values.
int SanitizeWordsToRemove(const std::set<std::string>& existing,
                          std::set<std::string>* to_remove) {
  DCHECK(to_remove);
  // Do not remove words that are missing from the dictionary.
  std::set<std::string> found_words =
      base::STLSetIntersection<std::set<std::string>>(existing, *to_remove);
  int result = VALID_CHANGE;
  if (to_remove->size() > found_words.size())
    result |= DETECTED_MISSING_WORDS;
  // Save the sanitized words to be removed.
  std::swap(*to_remove, found_words);
  return result;
}

}  // namespace

SpellcheckCustomDictionary::Change::Change() {
}

SpellcheckCustomDictionary::Change::~Change() {
}

void SpellcheckCustomDictionary::Change::AddWord(const std::string& word) {
  to_add_.insert(word);
}

void SpellcheckCustomDictionary::Change::AddWords(
    const std::set<std::string>& words) {
  to_add_.insert(words.begin(), words.end());
}

void SpellcheckCustomDictionary::Change::RemoveWord(const std::string& word) {
  to_remove_.insert(word);
}

int SpellcheckCustomDictionary::Change::Sanitize(
    const std::set<std::string>& words) {
  int result = VALID_CHANGE;
  if (!to_add_.empty())
    result |= SanitizeWordsToAdd(words, &to_add_);
  if (!to_remove_.empty())
    result |= SanitizeWordsToRemove(words, &to_remove_);
  return result;
}

SpellcheckCustomDictionary::SpellcheckCustomDictionary(
    const base::FilePath& dictionary_directory_name)
    : task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock()})),
      custom_dictionary_path_(
          dictionary_directory_name.Append(chrome::kCustomDictionaryFileName)),
      is_loaded_(false) {}

SpellcheckCustomDictionary::~SpellcheckCustomDictionary() {
}

const std::set<std::string>& SpellcheckCustomDictionary::GetWords() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return words_;
}

bool SpellcheckCustomDictionary::AddWord(const std::string& word) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<Change> dictionary_change(new Change);
  dictionary_change->AddWord(word);
  int result = dictionary_change->Sanitize(GetWords());
  Apply(*dictionary_change);
  Notify(*dictionary_change);
  Sync(*dictionary_change);
  Save(std::move(dictionary_change));
  return result == VALID_CHANGE;
}

bool SpellcheckCustomDictionary::RemoveWord(const std::string& word) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<Change> dictionary_change(new Change);
  dictionary_change->RemoveWord(word);
  int result = dictionary_change->Sanitize(GetWords());
  Apply(*dictionary_change);
  Notify(*dictionary_change);
  Sync(*dictionary_change);
  Save(std::move(dictionary_change));
  return result == VALID_CHANGE;
}

bool SpellcheckCustomDictionary::HasWord(const std::string& word) const {
  return base::Contains(words_, word);
}

void SpellcheckCustomDictionary::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void SpellcheckCustomDictionary::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

bool SpellcheckCustomDictionary::IsLoaded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return is_loaded_;
}

bool SpellcheckCustomDictionary::IsSyncing() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return !!sync_processor_.get();
}

void SpellcheckCustomDictionary::Load() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&SpellcheckCustomDictionary::LoadDictionaryFile,
                     custom_dictionary_path_),
      base::BindOnce(&SpellcheckCustomDictionary::OnLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SpellcheckCustomDictionary::WaitUntilReadyToSync(base::OnceClosure done) {
  DCHECK(!wait_until_ready_to_sync_cb_);
  if (is_loaded_)
    std::move(done).Run();
  else
    wait_until_ready_to_sync_cb_ = std::move(done);
}

syncer::SyncMergeResult SpellcheckCustomDictionary::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> sync_error_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!sync_processor_.get());
  DCHECK(!sync_error_handler_.get());
  DCHECK(sync_processor.get());
  DCHECK(sync_error_handler.get());
  DCHECK_EQ(syncer::DICTIONARY, type);
  sync_processor_ = std::move(sync_processor);
  sync_error_handler_ = std::move(sync_error_handler);

  // Build a list of words to add locally.
  std::unique_ptr<Change> to_change_locally(new Change);
  for (const syncer::SyncData& data : initial_sync_data) {
    DCHECK_EQ(syncer::DICTIONARY, data.GetDataType());
    to_change_locally->AddWord(data.GetSpecifics().dictionary().word());
  }

  // Add as many as possible local words remotely.
  to_change_locally->Sanitize(GetWords());
  Change to_change_remotely;
  to_change_remotely.AddWords(base::STLSetDifference<std::set<std::string>>(
      words_, to_change_locally->to_add()));

  // Add remote words locally.
  Apply(*to_change_locally);
  Notify(*to_change_locally);
  Save(std::move(to_change_locally));

  // Send local changes to the sync server.
  syncer::SyncMergeResult result(type);
  result.set_error(Sync(to_change_remotely));
  return result;
}

void SpellcheckCustomDictionary::StopSyncing(syncer::ModelType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(syncer::DICTIONARY, type);
  sync_processor_.reset();
  sync_error_handler_.reset();
}

syncer::SyncDataList SpellcheckCustomDictionary::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(syncer::DICTIONARY, type);
  syncer::SyncDataList data;
  size_t i = 0;
  for (const auto& word : words_) {
    if (i++ >= spellcheck::kMaxSyncableDictionaryWords)
      break;
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    data.push_back(syncer::SyncData::CreateLocalData(word, word, specifics));
  }
  return data;
}

syncer::SyncError SpellcheckCustomDictionary::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<Change> dictionary_change(new Change);
  for (const syncer::SyncChange& change : change_list) {
    DCHECK(change.IsValid());
    const std::string& word =
        change.sync_data().GetSpecifics().dictionary().word();
    switch (change.change_type()) {
      case syncer::SyncChange::ACTION_ADD:
        dictionary_change->AddWord(word);
        break;
      case syncer::SyncChange::ACTION_DELETE:
        dictionary_change->RemoveWord(word);
        break;
      case syncer::SyncChange::ACTION_UPDATE:
        // Intentionally fall through.
      case syncer::SyncChange::ACTION_INVALID:
        return sync_error_handler_->CreateAndUploadError(
            FROM_HERE,
            "Processing sync changes failed on change type " +
                syncer::SyncChange::ChangeTypeToString(change.change_type()));
    }
  }

  dictionary_change->Sanitize(GetWords());
  Apply(*dictionary_change);
  Notify(*dictionary_change);
  Save(std::move(dictionary_change));

  return syncer::SyncError();
}

SpellcheckCustomDictionary::LoadFileResult::LoadFileResult()
    : is_valid_file(false) {}

SpellcheckCustomDictionary::LoadFileResult::~LoadFileResult() {}

// static
std::unique_ptr<SpellcheckCustomDictionary::LoadFileResult>
SpellcheckCustomDictionary::LoadDictionaryFile(const base::FilePath& path) {
  std::unique_ptr<LoadFileResult> result = LoadDictionaryFileReliably(path);
  SpellCheckHostMetrics::RecordCustomWordCountStats(result->words.size());
  return result;
}

// static
void SpellcheckCustomDictionary::UpdateDictionaryFile(
    std::unique_ptr<Change> dictionary_change,
    const base::FilePath& path) {
  DCHECK(dictionary_change);

  if (dictionary_change->empty())
    return;

  std::unique_ptr<LoadFileResult> result = LoadDictionaryFileReliably(path);

  // Add words.
  result->words.insert(dictionary_change->to_add().begin(),
                       dictionary_change->to_add().end());

  // Remove words and save the remainder.
  SaveDictionaryFileReliably(
      path, base::STLSetDifference<std::set<std::string>>(
                result->words, dictionary_change->to_remove()));
}

void SpellcheckCustomDictionary::OnLoaded(
    std::unique_ptr<LoadFileResult> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(result);
  Change dictionary_change;
  dictionary_change.AddWords(result->words);
  dictionary_change.Sanitize(GetWords());
  Apply(dictionary_change);
  Sync(dictionary_change);
  is_loaded_ = true;
  if (wait_until_ready_to_sync_cb_)
    std::move(wait_until_ready_to_sync_cb_).Run();
  for (Observer& observer : observers_)
    observer.OnCustomDictionaryLoaded();
  if (!result->is_valid_file) {
    // Save cleaned up data only after startup.
    fix_invalid_file_.Reset(
        base::BindOnce(&SpellcheckCustomDictionary::FixInvalidFile,
                       weak_ptr_factory_.GetWeakPtr(), std::move(result)));
    base::PostTask(
        FROM_HERE,
        {content::BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
        fix_invalid_file_.callback());
  }
}

void SpellcheckCustomDictionary::Apply(const Change& dictionary_change) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!dictionary_change.to_add().empty()) {
    words_.insert(dictionary_change.to_add().begin(),
                  dictionary_change.to_add().end());
  }
  for (const auto& word : dictionary_change.to_remove())
    words_.erase(word);
}

void SpellcheckCustomDictionary::FixInvalidFile(
    std::unique_ptr<LoadFileResult> load_file_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SavePassedWordsToDictionaryFileReliably,
                     custom_dictionary_path_, std::move(load_file_result)));
}

void SpellcheckCustomDictionary::Save(
    std::unique_ptr<Change> dictionary_change) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  fix_invalid_file_.Cancel();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SpellcheckCustomDictionary::UpdateDictionaryFile,
                     std::move(dictionary_change), custom_dictionary_path_));
}

syncer::SyncError SpellcheckCustomDictionary::Sync(
    const Change& dictionary_change) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  syncer::SyncError error;
  if (!IsSyncing() || dictionary_change.empty())
    return error;

  // The number of words on the sync server should not exceed the limits.
  int server_size = static_cast<int>(words_.size()) -
      static_cast<int>(dictionary_change.to_add().size());
  int max_upload_size =
      std::max(0, static_cast<int>(spellcheck::kMaxSyncableDictionaryWords) -
                      server_size);
  int upload_size = std::min(
      static_cast<int>(dictionary_change.to_add().size()),
      max_upload_size);

  syncer::SyncChangeList sync_change_list;
  int i = 0;

  for (const auto& word : dictionary_change.to_add()) {
    if (i++ >= upload_size)
      break;
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    sync_change_list.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_ADD,
        syncer::SyncData::CreateLocalData(word, word, specifics)));
  }

  for (const std::string& word : dictionary_change.to_remove()) {
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    sync_change_list.push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_DELETE,
        syncer::SyncData::CreateLocalData(word, word, specifics)));
  }

  // Send the changes to the sync processor.
  error = sync_processor_->ProcessSyncChanges(FROM_HERE, sync_change_list);
  if (error.IsSet())
    return error;

  // Turn off syncing of this dictionary if the server already has the maximum
  // number of words.
  if (words_.size() > spellcheck::kMaxSyncableDictionaryWords)
    StopSyncing(syncer::DICTIONARY);

  return error;
}

void SpellcheckCustomDictionary::Notify(const Change& dictionary_change) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsLoaded() || dictionary_change.empty())
    return;
  for (Observer& observer : observers_)
    observer.OnCustomDictionaryChanged(dictionary_change);
}
