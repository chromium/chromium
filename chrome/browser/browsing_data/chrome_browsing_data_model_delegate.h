// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_MODEL_DELEGATE_H_
#define CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_MODEL_DELEGATE_H_

#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "content/public/browser/render_frame_host.h"

class ChromeBrowsingDataModelDelegate : public BrowsingDataModel::Delegate {
 public:
  // Storage types which are represented by the model. Some types have
  // incomplete implementations, and are marked as such.
  // TODO(crbug.com/1271155): Complete implementations for all browsing data.
  enum class StorageType {
    kTopics = static_cast<int>(BrowsingDataModel::StorageType::kLastType) +
              1,  // Not fetched from disk.
  };

  static std::unique_ptr<ChromeBrowsingDataModelDelegate> CreateForProfile(
      Profile* profile);
  static void BrowsingDataAccessed(content::RenderFrameHost* rfh,
                                   BrowsingDataModel::DataKey data_key,
                                   StorageType storage_type,
                                   bool blocked);
  explicit ChromeBrowsingDataModelDelegate(Profile* profile);
  ~ChromeBrowsingDataModelDelegate() override;

  // BrowsingDataModel::Delegate:
  void GetAllDataKeys(
      base::OnceCallback<void(std::vector<DelegateEntry>)> callback) override;
  void RemoveDataKey(BrowsingDataModel::DataKey data_key,
                     BrowsingDataModel::StorageTypeSet storage_types,
                     base::OnceClosure callback) override;

 private:
  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_MODEL_DELEGATE_H_
