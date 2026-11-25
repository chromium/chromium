// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_REGISTRY_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_INFOBARS_REGISTRY_INFOBAR_DELEGATE_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/infobars/confirm_infobar_creation_allowlist.h"
#include "chrome/browser/infobars/infobar_outcome_tracker.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/window_open_disposition.mojom-forward.h"
#include "ui/base/ui_base_types.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace gfx {
struct VectorIcon;
}

namespace infobars {

class InfoBar;
class InfoBarManager;

using SharedInfoBarSpec = base::RefCountedData<InfoBarSpec>;

// Adapts an immutable `InfoBarSpec` and per-show `InfoBarShowParams` to
// `ConfirmInfoBarDelegate`.
class RegistryInfoBarDelegate final : public ConfirmInfoBarDelegate,
                                      public content::WebContentsObserver {
 public:
  RegistryInfoBarDelegate(scoped_refptr<const SharedInfoBarSpec> spec,
                          content::WebContents* contents,
                          InfoBarShowParams params,
                          scoped_refptr<InfoBarOutcomeTracker> outcome,
                          base::RepeatingClosure on_global_close);
  RegistryInfoBarDelegate(const RegistryInfoBarDelegate&) = delete;
  RegistryInfoBarDelegate& operator=(const RegistryInfoBarDelegate&) = delete;
  ~RegistryInfoBarDelegate() override;

  using content::WebContentsObserver::web_contents;

  InfoBarOutcomeTracker* outcome() { return outcome_.get(); }
  bool in_interaction() const { return in_interaction_; }
  void set_close_after_interaction(bool close = true) {
    close_after_interaction_ = close;
  }

  // ConfirmInfoBarDelegate:
  InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool EqualsDelegate(InfoBarDelegate* delegate) const override;
  std::u16string GetMessageText() const override;
  std::u16string GetMessageTextTemplate() const override;
  base::span<const MessageSubstitution> GetMessageSubstitutions()
      const override;
  std::u16string GetLinkText() const override;
  std::optional<std::u16string> GetLinkAccessibleText() const override;
  GURL GetLinkURL() const override;
  bool ShouldShowLinkBeforeButton() const override;
  int GetLinkSpacingWhenPositionedBeforeButton() const override;
  int GetIconId() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  ui::ImageModel GetButtonImage(InfoBarButton button) const override;
  bool GetButtonEnabled(InfoBarButton button) const override;
  std::u16string GetButtonTooltip(InfoBarButton button) const override;
  std::optional<ui::ButtonStyle> GetButtonStyle(
      InfoBarButton button) const override;
  bool ShouldUseTextColorForButtonIcon(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  bool ExtraButtonPressed() override;
  void InfoBarDismissed() override;
  bool LinkClicked(ui::mojom::WindowOpenDisposition disposition) override;
  bool InlineSubstitutionLinkClicked(
      size_t index,
      ui::mojom::WindowOpenDisposition disposition) override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool ShouldHideInFullscreen() const override;
  bool ShouldAnimate() const override;
  bool IsCloseable() const override;
  InfoBarDelegate::InfobarPriority GetPriority() const override;

 private:
  const InfoBarSpec& spec() const { return spec_->data; }
  const InfoBarButtonParams* GetButtonParams(InfoBarButton button) const;
  bool RunAction(InfoBarSpec::ActionCallback callback,
                 InfoBarResult result,
                 bool should_close = true);

  const scoped_refptr<const SharedInfoBarSpec> spec_;
  InfoBarShowParams params_;
  const scoped_refptr<InfoBarOutcomeTracker> outcome_;
  base::RepeatingClosure on_global_close_;

  bool in_interaction_ = false;
  bool close_after_interaction_ = false;

  // Cached so substitutions stay stable across view rebuilds.
  mutable std::optional<std::vector<MessageSubstitution>> substitutions_;

  base::WeakPtrFactory<RegistryInfoBarDelegate> weak_factory_{this};
};

std::unique_ptr<InfoBar> CreateRegistryInfoBar(
    ConfirmInfoBarCreationAllowlist::PassKey pass_key,
    scoped_refptr<const SharedInfoBarSpec> spec,
    content::WebContents* contents,
    InfoBarShowParams params,
    scoped_refptr<InfoBarOutcomeTracker> outcome,
    base::RepeatingClosure on_global_close = base::RepeatingClosure());

// Defers removal until the current interaction returns if `infobar` is on the
// stack. Suppresses its pending outcome when `suppress_outcome` is true.
void RemoveRegistryInfoBar(InfoBarManager* manager,
                           InfoBar* infobar,
                           bool suppress_outcome);

}  // namespace infobars

#endif  // CHROME_BROWSER_INFOBARS_REGISTRY_INFOBAR_DELEGATE_H_
