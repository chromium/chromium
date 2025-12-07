// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IN_PROCESS_IMPORTER_BRIDGE_H_
#define CHROME_BROWSER_IMPORTER_IN_PROCESS_IMPORTER_BRIDGE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/importer/profile_writer.h"
#include "chrome/common/importer/importer_bridge.h"

class GURL;
class ExternalProcessImporterHost;

namespace user_data_importer {
struct ImportedBookmarkEntry;
}  // namespace user_data_importer

class InProcessImporterBridge : public ImporterBridge {
 public:
  InProcessImporterBridge(ProfileWriter* writer,
                          base::WeakPtr<ExternalProcessImporterHost> host);

  InProcessImporterBridge(const InProcessImporterBridge&) = delete;
  InProcessImporterBridge& operator=(const InProcessImporterBridge&) = delete;

  // Begin ImporterBridge implementation:
  void AddBookmarks(
      const std::vector<user_data_importer::ImportedBookmarkEntry>& bookmarks,
      const std::u16string& first_folder_name) override;

  void AddHomePage(const GURL& home_page) override;

  void SetFavicons(const favicon_base::FaviconUsageDataList& favicons) override;

  void SetHistoryItems(
      const std::vector<user_data_importer::ImporterURLRow>& rows,
      user_data_importer::VisitSource visit_source) override;

  void SetKeywords(
      const std::vector<user_data_importer::SearchEngineInfo>& search_engines,
      bool unique_on_host_and_path) override;

  void SetPasswordForm(
      const user_data_importer::ImportedPasswordForm& form) override;

  void SetAutofillFormData(
      const std::vector<ImporterAutofillFormDataEntry>& entries) override;

  void NotifyStarted() override;
  void NotifyItemStarted(user_data_importer::ImportItem item) override;
  void NotifyItemEnded(user_data_importer::ImportItem item) override;
  void NotifyEnded() override;

  std::u16string GetLocalizedString(int message_id) override;
  // End ImporterBridge implementation.

 private:
  ~InProcessImporterBridge() override;

  const raw_ptr<ProfileWriter, DanglingUntriaged> writer_;  // weak
  const base::WeakPtr<ExternalProcessImporterHost> host_;
};

#endif  // CHROME_BROWSER_IMPORTER_IN_PROCESS_IMPORTER_BRIDGE_H_
