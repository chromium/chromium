// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ARC_ARC_INTENT_HELPER_MOJO_LACROS_H_
#define CHROME_BROWSER_LACROS_ARC_ARC_INTENT_HELPER_MOJO_LACROS_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/arc.mojom.h"
#include "components/arc/common/intent_helper/arc_intent_helper_mojo_delegate.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace arc {

class ArcIntentHelperMojoLacros : public ArcIntentHelperMojoDelegate {
 public:
  ArcIntentHelperMojoLacros();
  ArcIntentHelperMojoLacros(const ArcIntentHelperMojoLacros&) = delete;
  ArcIntentHelperMojoLacros operator=(const ArcIntentHelperMojoLacros&) =
      delete;
  ~ArcIntentHelperMojoLacros() override;

  // arc::ArcIntentHelperMojoDelegate:
  // Returns true if ARC is available.
  bool IsArcAvailable() override;
  bool IsRequestUrlHandlerListAvailable() override;
  bool IsRequestTextSelectionActionsAvailable() override;

  // Calls RequestUrlHandlerList mojo API.
  bool RequestUrlHandlerList(const std::string& url,
                             RequestUrlHandlerListCallback callback) override;
  bool RequestTextSelectionActions(
      const std::string& text,
      ui::ResourceScaleFactor scale_factor,
      RequestTextSelectionActionsCallback callback) override;

  // Calls HandleUrl mojo API.
  bool HandleUrl(const std::string& url,
                 const std::string& package_name) override;
  bool HandleIntent(const IntentInfo& intent,
                    const ActivityName& activity) override;
  bool AddPreferredPackage(const std::string& package_name) override;

 private:
  // Convert vector of crosapi::mojom::IntentHandlerInfoPtr to vector of
  // ArcIconCacheDelegate::IntentHandlerInfo.
  void OnRequestUrlHandlerList(
      RequestUrlHandlerListCallback callback,
      std::vector<crosapi::mojom::IntentHandlerInfoPtr> handlers,
      crosapi::mojom::RequestUrlHandlerListStatus status);

  // Convert vector of crosapi::mojom::TextSelectionActionPtr to vector of
  // arc::TextSelectionActionDelegate::TextSelectionAction.
  void OnRequestTextSelectionActions(
      RequestTextSelectionActionsCallback callback,
      crosapi::mojom::RequestTextSelectionActionsStatus status,
      std::vector<crosapi::mojom::TextSelectionActionPtr> actions);

  // This must come last to make sure weak pointers are invalidated first.
  base::WeakPtrFactory<ArcIntentHelperMojoLacros> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_LACROS_ARC_ARC_INTENT_HELPER_MOJO_LACROS_H_
