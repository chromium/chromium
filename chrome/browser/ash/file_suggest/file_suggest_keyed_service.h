// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGEST_KEYED_SERVICE_H_
#define CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGEST_KEYED_SERVICE_H_

#include <optional>
#include <utility>
#include <vector>

#include "ash/utility/persistent_proto.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ash/app_list/search/ranking/removed_results.pb.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace app_list {
class RemovedResultsRanker;
class ZeroStateDriveProvider;
}  // namespace app_list

namespace ash {
class FileSuggestionProvider;
class LocalFileSuggestionProvider;
struct SearchResultMetadata;

// The keyed service that queries for the file suggestions (for both the drive
// files and local files) and exposes those data to consumers such as app list.
// TODO(https://crbug.com/1356347): move this service to a neutral place rather
// than leaving it under the app list directory.
class FileSuggestKeyedService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when file suggestions change.
    virtual void OnFileSuggestionUpdated(FileSuggestionType type) {}
  };

  FileSuggestKeyedService(Profile* profile,
                          PersistentProto<app_list::RemovedResultsProto> proto);
  FileSuggestKeyedService(const FileSuggestKeyedService&) = delete;
  FileSuggestKeyedService& operator=(const FileSuggestKeyedService&) = delete;
  ~FileSuggestKeyedService() override;

  // Requests to update the item suggest cache. Only used by the zero state
  // drive provider. Overridden for tests.
  // TODO(https://crbug.com/1356347): Now the app list relies on this service to
  // fetch the drive suggestion data. Meanwhile, this service relies on the app
  // list to trigger the item cache update. This cyclic dependency could be
  // confusing. The service should update the data cache by its own without
  // depending on the app list code.
  virtual void MaybeUpdateItemSuggestCache(
      base::PassKey<app_list::ZeroStateDriveProvider>);

  // Queries for the suggested files of the specified type and returns the
  // suggested file data, including file paths and suggestion reasons, through
  // the callback. The returned suggestions have been filtered by the file
  // last modification time. Only the files that have been modified more
  // recently than a threshold are returned. Overridden for tests.
  virtual void GetSuggestFileData(FileSuggestionType type,
                                  GetSuggestFileDataCallback callback);

  // Persists the ids of the suggestions specified by `absolute_file_paths` so
  // that the corresponding suggestions are not sent to service clients anymore.
  // Also notifies observers of suggestion updates. Overridden for tests.
  virtual void RemoveSuggestionsAndNotify(
      const std::vector<base::FilePath>& absolute_file_paths);

  // Similar to `RemoveSuggestionsAndNotify()` but with the difference that
  // the suggestion is specified by a search result. Overridden for tests.
  virtual void RemoveSuggestionBySearchResultAndNotify(
      const SearchResultMetadata& search_result);

  // Used to expose `proto_` to app list so that the app list can query/remove
  // non-file result ids from `proto_`.
  // TODO(https://crbug.com/1368833): remove this function when the removed file
  // results are managed by this service's own proto without reusing the app
  // list's.
  PersistentProto<app_list::RemovedResultsProto>* GetProto(
      base::PassKey<app_list::RemovedResultsRanker>);

  // Adds/Removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if the service is ready to provide all types of suggestions.
  bool IsReadyForTest() const;

  FileSuggestionProvider* drive_file_suggestion_provider_for_test() {
    return drive_file_suggestion_provider_.get();
  }

  LocalFileSuggestionProvider* local_file_suggestion_provider_for_test() {
    return local_file_suggestion_provider_.get();
  }

 protected:
  // Called whenever a suggestion provider updates.
  void OnSuggestionProviderUpdated(FileSuggestionType type);

  // Filters `suggestions` so that any suggestons which have a duplicate file
  // path will be removed. Then returns the filtered result through `callback`.
  void FilterDuplicateSuggestions(
      GetSuggestFileDataCallback callback,
      const std::optional<std::vector<FileSuggestData>>& suggestions);

  // Filters `suggestions` so that the suggestions that were removed before do
  // not appear. Then returns the filtered result through `callback`.
  void FilterRemovedSuggestions(
      GetSuggestFileDataCallback callback,
      const std::optional<std::vector<FileSuggestData>>& suggestions);

  // Returns whether `proto_` is initialized.
  bool IsProtoInitialized() const;

 private:
  // Called when `proto_` is ready to read.
  void OnRemovedSuggestionProtoReady();

  // Removes the suggestions specified by type-id pairs.
  void RemoveSuggestionsByTypeIdPairs(
      const std::vector<std::pair<FileSuggestionType, std::string>>&
          type_id_pairs);

  // The provider of drive file suggestions.
  std::unique_ptr<FileSuggestionProvider> drive_file_suggestion_provider_;

  // The provider of local file suggestions.
  std::unique_ptr<LocalFileSuggestionProvider> local_file_suggestion_provider_;

  base::ObserverList<Observer> observers_;

  const raw_ptr<Profile> profile_;

  // Used to query/persis the removed result ids. NOTE: `proto_` contains
  // non-file ids.
  // TODO(https://crbug.com/1368833): `proto_` should only contain file ids
  // after this issue gets fixed.
  PersistentProto<app_list::RemovedResultsProto> proto_;

  base::WeakPtrFactory<FileSuggestKeyedService> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGEST_KEYED_SERVICE_H_
