// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_page_action_controller.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notimplemented.h"
#include "chrome/browser/indigo/indigo_alpha_rpc.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/common/chrome_features.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition.h"

namespace indigo {

DEFINE_USER_DATA(IndigoPageActionController);

IndigoPageActionController::IndigoPageActionController(
    tabs::TabInterface& tab_interface,
    page_actions::PageActionController& page_action_controller)
    : tabs::ContentsObservingTabFeature(tab_interface),
      page_action_controller_(page_action_controller),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
  UpdateEntryPointsState();
}

IndigoPageActionController::~IndigoPageActionController() = default;

// static
IndigoPageActionController* IndigoPageActionController::From(
    tabs::TabInterface* tab) {
  if (!tab) {
    return nullptr;
  }
  return Get(tab->GetUnownedUserDataHost());
}

void IndigoPageActionController::InvokeAction() {
  base::RecordAction(base::UserMetricsAction("Indigo.PageAction.Click"));

  // TODO: b/482792874 - Analyze the page and act on it, instead of just opening
  // a tab based on a fixed input.
  content::WebContents* web_contents = tab().GetContents();
  if (!web_contents) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile) {
    return;
  }

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();
  ExecuteAlphaGenerateRpc(
      loader_factory.get(),
      base::BindOnce(
          [](base::WeakPtr<BrowserWindowInterface> window,
             base::expected<GURL, AlphaGenerateError> result) {
            if (window && result.has_value()) {
              window->OpenGURL(result.value(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB);
            } else if (!result.has_value()) {
              LOG(ERROR) << "Indigo alpha generate error "
                         << result.error().error_type << ": "
                         << result.error().error_message;
            }
          },
          tab().GetBrowserWindowInterface()->GetWeakPtr()));
}

void IndigoPageActionController::UpdateEntryPointsState() {
  CHECK(base::FeatureList::IsEnabled(features::kIndigo));

  page_action_controller_->Show(kActionIndigo);
  page_action_controller_->ShowSuggestionChip(kActionIndigo);

  base::RecordAction(base::UserMetricsAction("Indigo.PageAction.Show"));
}

}  // namespace indigo
