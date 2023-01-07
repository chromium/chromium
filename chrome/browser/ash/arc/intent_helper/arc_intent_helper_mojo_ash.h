// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INTENT_HELPER_ARC_INTENT_HELPER_MOJO_ASH_H_
#define CHROME_BROWSER_ASH_ARC_INTENT_HELPER_ARC_INTENT_HELPER_MOJO_ASH_H_

#include <string>
#include <vector>

#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/common/intent_helper/arc_intent_helper_mojo_delegate.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace arc {

// Ash-side ArcIntentHelperMojoDelegate handling.
class ArcIntentHelperMojoAsh : public ArcIntentHelperMojoDelegate {
 public:
  ArcIntentHelperMojoAsh();
  ArcIntentHelperMojoAsh(const ArcIntentHelperMojoAsh&) = delete;
  ArcIntentHelperMojoAsh& operator=(const ArcIntentHelperMojoAsh&) = delete;
  ~ArcIntentHelperMojoAsh() override;

  // ArcIntentHelperMojoDelegate:
  // Returns true if ARC is available.
  bool IsArcAvailable() override;
  bool IsRequestUrlHandlerListAvailable() override;
  bool IsRequestTextSelectionActionsAvailable() override;

  bool RequestUrlHandlerList(const std::string& url,
                             RequestUrlHandlerListCallback callback) override;
  bool RequestTextSelectionActions(
      const std::string& text,
      ui::ResourceScaleFactor scale_factor,
      RequestTextSelectionActionsCallback callback) override;
  bool HandleUrl(const std::string& url,
                 const std::string& package_name) override;
  bool HandleIntent(const IntentInfo& intent,
                    const ActivityName& activity) override;
  bool AddPreferredPackage(const std::string& package_name) override;

 private:
  // Convert vector of mojom::IntentHandlerInfoPtr to vector of
  // ArcIconCacheDelegate::IntentHandlerInfo.
  void OnRequestUrlHandlerList(
      RequestUrlHandlerListCallback callback,
      std::vector<mojom::IntentHandlerInfoPtr> handlers);

  // Convertvector of mojom::TextSelectionActionPtr to vector of
  // ArcIntentHelperMojoDelegate::TextSelectionAction.
  void OnRequestTextSelectionActions(
      RequestTextSelectionActionsCallback callback,
      std::vector<mojom::TextSelectionActionPtr> actions);

  // Convert arc::mojom::TextSelectionAction into TextSelectionAction.
  void ConvertTextSelectionAction(TextSelectionAction** converted_action,
                                  mojom::TextSelectionActionPtr action,
                                  base::OnceClosure callback,
                                  const gfx::ImageSkia& image);

  // This must come last to make sure weak pointers are invalidated first.
  base::WeakPtrFactory<ArcIntentHelperMojoAsh> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INTENT_HELPER_ARC_INTENT_HELPER_MOJO_ASH_H_
