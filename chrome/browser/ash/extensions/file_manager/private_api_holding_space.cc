// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_holding_space.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/file_manager/indexing/file_index.h"
#include "chromeos/ash/components/file_manager/indexing/file_index_service.h"
#include "chromeos/ash/components/file_manager/indexing/file_index_service_registry.h"
#include "chromeos/ash/components/file_manager/indexing/file_info.h"
#include "chromeos/ash/components/file_manager/indexing/term.h"
#include "components/user_manager/user_manager.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

namespace extensions {

FileManagerPrivateInternalToggleAddedToHoldingSpaceFunction::
    FileManagerPrivateInternalToggleAddedToHoldingSpaceFunction() = default;

FileManagerPrivateInternalToggleAddedToHoldingSpaceFunction::
    ~FileManagerPrivateInternalToggleAddedToHoldingSpaceFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalToggleAddedToHoldingSpaceFunction::Run() {
  using extensions::api::file_manager_private_internal::
      ToggleAddedToHoldingSpace::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ash::HoldingSpaceKeyedService* const holding_space =
      ash::HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          browser_context());
  if (!holding_space) {
    return RespondNow(Error("Not enabled"));
  }

  const user_manager::User* user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
          browser_context());

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());

  std::vector<storage::FileSystemURL> file_system_urls;
  for (const auto& item_url : params->urls) {
    const storage::FileSystemURL file_system_url =
        file_system_context->CrackURLInFirstPartyContext(GURL(item_url));
    if (!file_system_url.is_valid()) {
      return RespondNow(Error("Invalid item URL " + item_url));
    }
    file_system_urls.push_back(file_system_url);
  }

  if (params->add) {
    std::erase_if(
        file_system_urls,
        [holding_space](const storage::FileSystemURL& file_system_url) {
          return holding_space->ContainsPinnedFile(file_system_url);
        });
    holding_space->AddPinnedFiles(
        file_system_urls, ash::holding_space_metrics::EventSource::kFilesApp);
  } else {
    std::erase_if(
        file_system_urls,
        [holding_space](const storage::FileSystemURL& file_system_url) {
          return !holding_space->ContainsPinnedFile(file_system_url);
        });
    holding_space->RemovePinnedFiles(
        file_system_urls, ash::holding_space_metrics::EventSource::kFilesApp);
  }

  // Also send the data to the File Index.
  if (base::FeatureList::IsEnabled(ash::features::kFilesMaterializedViews)) {
    // TODO(b/327534188): Use the MaterializedViews API.
    auto* registry = ::ash::file_manager::FileIndexServiceRegistry::Get();
    ::ash::file_manager::FileIndexService* index =
        registry ? registry->GetFileIndexService(user->GetAccountId())
                 : nullptr;
    if (index) {
      ::ash::file_manager::Term starred("label", u"starred");
      if (params->add) {
        for (const auto& url : file_system_urls) {
          ::ash::file_manager::FileInfo info(url.ToGURL(), 0, base::Time());
          LOG(ERROR) << "Adding terms: " << starred.token()
                     << " url: " << url.ToGURL().spec();
          index->PutFileInfo(
              info,
              base::BindOnce(
                  [](::ash::file_manager::FileIndexService* index,
                     const storage::FileSystemURL& url,
                     const ::ash::file_manager::Term& term,
                     ::ash::file_manager::OpResults r) {
                    index->AddTerms(
                        {term}, url.ToGURL(),
                        base::BindOnce([](::ash::file_manager::OpResults r) {
                          LOG(ERROR) << "result: " << r;
                        }));
                  },
                  index, url, starred));
        }
      } else {
        for (const auto& url : file_system_urls) {
          LOG(ERROR) << "Removing terms: " << starred.token()
                     << " url: " << url.ToGURL().spec();
          index->RemoveTerms(
              {starred}, url.ToGURL(),
              base::BindOnce([](::ash::file_manager::OpResults r) {
                LOG(ERROR) << "result: " << r;
              }));
        }
      }
    }
  }

  return RespondNow(NoArguments());
}

FileManagerPrivateGetHoldingSpaceStateFunction::
    FileManagerPrivateGetHoldingSpaceStateFunction() = default;

FileManagerPrivateGetHoldingSpaceStateFunction::
    ~FileManagerPrivateGetHoldingSpaceStateFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateGetHoldingSpaceStateFunction::Run() {
  ash::HoldingSpaceKeyedService* const holding_space =
      ash::HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          browser_context());
  if (!holding_space) {
    return RespondNow(Error("Not enabled"));
  }

  std::vector<GURL> items = holding_space->GetPinnedFiles();

  api::file_manager_private::HoldingSpaceState holding_space_state;
  for (const auto& item : items) {
    holding_space_state.item_urls.push_back(item.spec());
  }

  if (base::FeatureList::IsEnabled(ash::features::kFilesMaterializedViews)) {
    const user_manager::User* user =
        ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
            browser_context());
    auto* registry = ::ash::file_manager::FileIndexServiceRegistry::Get();
    ::ash::file_manager::FileIndexService* index =
        registry ? registry->GetFileIndexService(user->GetAccountId())
                 : nullptr;
    if (!index) {
      LOG(ERROR) << "Couldn't get the file index";
    } else {
      ::ash::file_manager::Term starred("label", u"starred");
      ::ash::file_manager::Query q({starred});
      index->Search(
          q,
          base::BindOnce(
              &FileManagerPrivateGetHoldingSpaceStateFunction::OnSearchResult,
              this));
      return RespondLater();
    }
  }

  return RespondNow(ArgumentList(
      api::file_manager_private::GetHoldingSpaceState::Results::Create(
          holding_space_state)));
}

void FileManagerPrivateGetHoldingSpaceStateFunction::OnSearchResult(
    ::ash::file_manager::SearchResults result) {
  api::file_manager_private::HoldingSpaceState holding_space_state;
  for (const auto& m : result.matches) {
    holding_space_state.item_urls.push_back(m.file_info.file_url.spec());
  }

  Respond(ArgumentList(
      api::file_manager_private::GetHoldingSpaceState::Results::Create(
          holding_space_state)));
}

}  // namespace extensions
