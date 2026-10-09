// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/browser_infobar_manager.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace infobars {

// RegistryInfoBarDelegate acts as the universal adapter between the modern
// InfoBarSpec and the legacy ConfirmInfoBarDelegate.
class RegistryInfoBarDelegate final : public ConfirmInfoBarDelegate,
                                      public content::WebContentsObserver {
 public:
  // The delegate observes `contents` because substitutions are read while
  // the view is being built, before the infobar has an owner. Values in
  // `params` take precedence over the spec for this instance.
  RegistryInfoBarDelegate(InfoBarSpec spec,
                          content::WebContents* contents,
                          InfoBarShowParams params,
                          BrowserInfoBarManager& manager)
      : content::WebContentsObserver(contents),
        spec_(std::move(spec)),
        params_(std::move(params)),
        manager_(manager) {
    if (params_.substitutions.has_value()) {
      substitutions_ = std::move(*params_.substitutions);
    }
  }

  ~RegistryInfoBarDelegate() override {
    // Reports whatever outcome is still owed. Interactions report eagerly
    // and clear it; manager-initiated removals clear it too, and delegates
    // that never made it on screen owe nothing.
    if (pending_result_) {
      if (*pending_result_ == InfoBarResult::kIgnored) {
        base::UmaHistogramSparse("InfoBar.Centralized.Ignored",
                                 GetIdentifier());
      }
      RunResultCallback(*pending_result_);
    }
  }

  // Called once the infobar has actually been added.
  void set_shown(
      std::optional<InfoBarResult> pending_result = InfoBarResult::kIgnored) {
    pending_result_ = pending_result;
  }

  // Keeps a manager-initiated removal from being reported as an outcome.
  void suppress_result() { pending_result_.reset(); }

  bool in_interaction() const { return in_interaction_; }
  void set_close_after_interaction(bool close = true) {
    close_after_interaction_ = close;
  }

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return spec_.identifier();
  }

  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override {
    if (spec_.allow_duplicates()) {
      return false;
    }
    return ConfirmInfoBarDelegate::EqualsDelegate(delegate);
  }

  std::u16string GetMessageText() const override {
    if (params_.message_text.has_value()) {
      return *params_.message_text;
    }
    if (!spec_.message_text_template().empty()) {
      std::vector<std::u16string> substitution_texts;
      for (const MessageSubstitution& substitution :
           GetMessageSubstitutions()) {
        substitution_texts.push_back(substitution.text);
      }
      return base::ReplaceStringPlaceholders(spec_.message_text_template(),
                                             substitution_texts,
                                             /*offsets=*/nullptr);
    }
    return spec_.message_text();
  }

  std::u16string GetMessageTextTemplate() const override {
    if (params_.message_text.has_value()) {
      return std::u16string();
    }
    return spec_.message_text_template();
  }

  base::span<const MessageSubstitution> GetMessageSubstitutions()
      const override {
    if (!substitutions_.has_value()) {
      substitutions_ = spec_.substitutions_callback()
                           ? spec_.substitutions_callback().Run(web_contents())
                           : std::vector<MessageSubstitution>();
    }
    return *substitutions_;
  }

  std::u16string GetLinkText() const override {
    return params_.link_text.has_value() ? *params_.link_text
                                         : spec_.link_text();
  }

  std::optional<std::u16string> GetLinkAccessibleText() const override {
    return spec_.link_accessible_text();
  }

  GURL GetLinkURL() const override { return spec_.link_navigation_url(); }

  bool ShouldShowLinkBeforeButton() const override {
    return spec_.should_show_link_before_button();
  }

  int GetLinkSpacingWhenPositionedBeforeButton() const override {
    return spec_.link_spacing_when_positioned_before_button();
  }

  int GetIconId() const override { return spec_.icon_id(); }

  const gfx::VectorIcon& GetVectorIcon() const override {
    if (dark_mode() && spec_.dark_mode_icon()) {
      return *spec_.dark_mode_icon();
    }
    return spec_.icon() ? *spec_.icon()
                        : ConfirmInfoBarDelegate::GetVectorIcon();
  }

  int GetButtons() const override {
    int buttons = BUTTON_NONE;
    const std::u16string& ok_label = params_.ok_button.label.has_value()
                                         ? *params_.ok_button.label
                                         : spec_.ok_button_label();
    if (!ok_label.empty() || spec_.ok_button_callback() ||
        params_.ok_button_callback) {
      buttons |= BUTTON_OK;
    }
    const std::u16string& cancel_label = params_.cancel_button.label.has_value()
                                             ? *params_.cancel_button.label
                                             : spec_.cancel_button_label();
    if (!cancel_label.empty() || spec_.cancel_button_callback() ||
        params_.cancel_button_callback) {
      buttons |= BUTTON_CANCEL;
    }
    const std::u16string& extra_label = params_.extra_button.label.has_value()
                                            ? *params_.extra_button.label
                                            : spec_.extra_button_label();
    if (!extra_label.empty() || spec_.extra_button_callback() ||
        params_.extra_button_callback) {
      buttons |= BUTTON_EXTRA;
    }
    return buttons;
  }

  std::u16string GetButtonLabel(InfoBarButton button) const override {
    if (button == BUTTON_OK) {
      const std::u16string& label = params_.ok_button.label.has_value()
                                        ? *params_.ok_button.label
                                        : spec_.ok_button_label();
      if (!label.empty()) {
        return label;
      }
    }
    if (button == BUTTON_CANCEL) {
      const std::u16string& label = params_.cancel_button.label.has_value()
                                        ? *params_.cancel_button.label
                                        : spec_.cancel_button_label();
      if (!label.empty()) {
        return label;
      }
    }
    if (button == BUTTON_EXTRA) {
      const std::u16string& label = params_.extra_button.label.has_value()
                                        ? *params_.extra_button.label
                                        : spec_.extra_button_label();
      if (!label.empty()) {
        return label;
      }
    }
    return ConfirmInfoBarDelegate::GetButtonLabel(button);
  }

  ui::ImageModel GetButtonImage(InfoBarButton button) const override {
    if (const auto* p = GetButtonParams(button); p && p->image.has_value()) {
      return *p->image;
    }
    return ConfirmInfoBarDelegate::GetButtonImage(button);
  }

  bool GetButtonEnabled(InfoBarButton button) const override {
    if (const auto* p = GetButtonParams(button); p && p->enabled.has_value()) {
      return *p->enabled;
    }
    return ConfirmInfoBarDelegate::GetButtonEnabled(button);
  }

  std::u16string GetButtonTooltip(InfoBarButton button) const override {
    if (const auto* p = GetButtonParams(button); p && p->tooltip.has_value()) {
      return *p->tooltip;
    }
    return ConfirmInfoBarDelegate::GetButtonTooltip(button);
  }

  std::optional<ui::ButtonStyle> GetButtonStyle(
      InfoBarButton button) const override {
    if (const auto* p = GetButtonParams(button); p && p->style.has_value()) {
      return p->style;
    }
    return ConfirmInfoBarDelegate::GetButtonStyle(button);
  }

  bool ShouldUseTextColorForButtonIcon(InfoBarButton button) const override {
    if (const auto* p = GetButtonParams(button);
        p && p->use_text_color_for_icon.has_value()) {
      return *p->use_text_color_for_icon;
    }
    return ConfirmInfoBarDelegate::ShouldUseTextColorForButtonIcon(button);
  }

  bool Accept() override {
    base::UmaHistogramSparse("InfoBar.Centralized.Accept", GetIdentifier());
    return RunAction(params_.ok_button_callback ? params_.ok_button_callback
                                                : spec_.ok_button_callback(),
                     InfoBarResult::kAccepted, spec_.close_on_accept());
  }

  bool Cancel() override {
    base::UmaHistogramSparse("InfoBar.Centralized.Cancel", GetIdentifier());
    return RunAction(params_.cancel_button_callback
                         ? params_.cancel_button_callback
                         : spec_.cancel_button_callback(),
                     InfoBarResult::kCancelled, spec_.close_on_cancel());
  }

  bool ExtraButtonPressed() override {
    base::UmaHistogramSparse("InfoBar.Centralized.Extra", GetIdentifier());
    return RunAction(
        params_.extra_button_callback ? params_.extra_button_callback
                                      : spec_.extra_button_callback(),
        InfoBarResult::kExtraButtonPressed, spec_.close_on_extra_button());
  }

  void InfoBarDismissed() override {
    base::UmaHistogramSparse("InfoBar.Centralized.Dismiss", GetIdentifier());
    RunAction(spec_.dismiss_callback(), InfoBarResult::kDismissed);
  }

  bool LinkClicked(WindowOpenDisposition disposition) override {
    base::AutoReset<bool> in_interaction(&in_interaction_, true);
    MarkLinkClicked();
    base::UmaHistogramSparse("InfoBar.Centralized.LinkClicked",
                             GetIdentifier());
    return ConfirmInfoBarDelegate::LinkClicked(disposition) ||
           close_after_interaction_;
  }

  bool InlineSubstitutionLinkClicked(
      size_t index,
      WindowOpenDisposition disposition) override {
    base::AutoReset<bool> in_interaction(&in_interaction_, true);
    MarkLinkClicked();
    base::UmaHistogramSparse("InfoBar.Centralized.LinkClicked",
                             GetIdentifier());
    const InfoBarSpec::InlineLinkCallback& callback =
        params_.inline_link_callback ? params_.inline_link_callback
                                     : spec_.inline_link_callback();
    const bool should_close =
        callback && callback.Run(web_contents(), index, disposition);
    if (should_close) {
      const bool should_report = ClaimPendingResult();
      if (IsGlobal()) {
        manager_->Hide(GetIdentifier());
      }
      if (should_report) {
        RunResultCallback(InfoBarResult::kLinkClicked);
      }
    }
    return should_close || close_after_interaction_;
  }

  bool ShouldExpire(const NavigationDetails& details) const override {
    return spec_.expire_on_navigation() &&
           ConfirmInfoBarDelegate::ShouldExpire(details);
  }

  bool ShouldHideInFullscreen() const override {
    return spec_.should_hide_in_fullscreen();
  }

  bool ShouldAnimate() const override { return spec_.should_animate(); }

  bool IsCloseable() const override { return spec_.is_closeable(); }

  InfoBarDelegate::InfobarPriority GetPriority() const override {
    return spec_.priority();
  }

 private:
  const InfoBarButtonParams* GetButtonParams(InfoBarButton button) const {
    switch (button) {
      case BUTTON_OK:
        return &params_.ok_button;
      case BUTTON_CANCEL:
        return &params_.cancel_button;
      case BUTTON_EXTRA:
        return &params_.extra_button;
      case BUTTON_NONE:
        return nullptr;
    }
    return nullptr;
  }

  bool IsGlobal() const {
    return params_.scope.value_or(spec_.scope()) == InfoBarScope::kGlobal;
  }

  // `callback` is taken by value since it may destroy `this`.
  bool RunAction(InfoBarSpec::ActionCallback callback,
                 InfoBarResult result,
                 bool should_close = true) {
    in_interaction_ = true;
    const bool should_report = ClaimPendingResult();
    if (should_close && IsGlobal()) {
      manager_->Hide(GetIdentifier());
    }
    if (should_report) {
      // Reported by the destructor if `callback` destroys `this`.
      pending_result_ = result;
    }
    auto weak_this = weak_factory_.GetWeakPtr();
    if (auto* contents = web_contents(); contents && callback) {
      callback.Run(contents);
    }
    if (!weak_this) {
      return false;
    }
    in_interaction_ = false;
    if (should_report) {
      pending_result_.reset();
      RunResultCallback(result);
    }
    return should_close || close_after_interaction_;
  }

  std::optional<InfoBarResult>* ActivePendingResult() {
    if (!IsGlobal()) {
      return &pending_result_;
    }
    auto it = manager_->active_global_infobars_.find(GetIdentifier());
    return it != manager_->active_global_infobars_.end()
               ? &it->second.pending_result
               : nullptr;
  }

  void MarkLinkClicked() {
    if (auto* pending = ActivePendingResult(); pending && *pending) {
      *pending = InfoBarResult::kLinkClicked;
    }
  }

  bool ClaimPendingResult() {
    auto* pending = ActivePendingResult();
    if (!pending || !*pending) {
      return false;
    }
    pending->reset();
    return true;
  }

  void RunResultCallback(InfoBarResult result) {
    const InfoBarSpec::ResultCallback& callback = params_.result_callback
                                                      ? params_.result_callback
                                                      : spec_.result_callback();
    if (callback) {
      callback.Run(web_contents(), result);
    }
  }

  InfoBarSpec spec_;
  InfoBarShowParams params_;
  const raw_ref<BrowserInfoBarManager> manager_;

  // The terminal outcome still owed at destruction, cleared once an
  // interaction reports its own result.
  std::optional<InfoBarResult> pending_result_;

  bool in_interaction_ = false;
  bool close_after_interaction_ = false;

  // Computed once and cached so the substitutions don't change under the
  // view.
  mutable std::optional<std::vector<MessageSubstitution>> substitutions_;

  base::WeakPtrFactory<RegistryInfoBarDelegate> weak_factory_{this};
};

namespace {

std::unique_ptr<infobars::InfoBar> CreateInfoBarForSpec(
    const InfoBarSpec& spec,
    content::WebContents* contents,
    InfoBarShowParams params,
    BrowserInfoBarManager& manager) {
  const InfoBarSpec::CustomViewCallback& custom_view_callback =
      params.custom_view_callback ? params.custom_view_callback
                                  : spec.custom_view_callback();
  std::unique_ptr<views::View> custom_view =
      custom_view_callback ? custom_view_callback.Run(contents) : nullptr;
  return CreateConfirmInfoBar(std::make_unique<RegistryInfoBarDelegate>(
                                  spec, contents, std::move(params), manager),
                              std::move(custom_view));
}

content::WebContents* GetActiveWebContents() {
  // TODO(crbug.com/512825363): Derivation of browser will be changed to
  // accommodate profile.
  auto* browser = GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  if (!browser) {
    return nullptr;
  }

  auto* tab = browser->GetActiveTabInterface();
  if (!tab) {
    return nullptr;
  }

  return tab->GetContents();
}

// Removes `infobar` without reporting a result.
void RemoveInfoBarWithoutResult(infobars::InfoBarManager* manager,
                                infobars::InfoBar* infobar) {
  auto* delegate = static_cast<RegistryInfoBarDelegate*>(infobar->delegate());
  delegate->suppress_result();
  if (delegate->in_interaction()) {
    delegate->set_close_after_interaction();
    return;
  }
  manager->RemoveInfoBar(infobar);
}

}  // namespace

DEFINE_USER_DATA(BrowserInfoBarManager);

BrowserInfoBarManager::BrowserInfoBarManager(BrowserProcess* browser_process)
    : scoped_unowned_user_data_(browser_process->GetUnownedUserDataHost(),
                                *this) {
  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
  GlobalBrowserCollection::GetInstance()->ForEach(
      [this](BrowserWindowInterface* browser) {
        OnBrowserCreated(browser);
        return true;
      });
}

BrowserInfoBarManager::~BrowserInfoBarManager() = default;

// static
BrowserInfoBarManager* BrowserInfoBarManager::From(
    BrowserProcess* browser_process) {
  return Get(browser_process->GetUnownedUserDataHost());
}

void BrowserInfoBarManager::Register(InfoBarSpec spec) {
  CHECK(!registered_specs_.contains(spec.identifier()));
  registered_specs_[spec.identifier()] = std::move(spec);
}

bool BrowserInfoBarManager::IsRegistered(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) const {
  return registered_specs_.contains(identifier);
}

infobars::InfoBar* BrowserInfoBarManager::Show(
    tabs::TabInterface* tab,
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  return Show(tab, identifier, InfoBarShowParams());
}

infobars::InfoBar* BrowserInfoBarManager::Show(
    tabs::TabInterface* tab,
    infobars::InfoBarDelegate::InfoBarIdentifier identifier,
    InfoBarShowParams params) {
  auto it = registered_specs_.find(identifier);
  if (it == registered_specs_.end()) {
    return nullptr;
  }
  CHECK(tab);
  CHECK(params.scope.value_or(it->second.scope()) == InfoBarScope::kTab);

  auto* contents = tab->GetContents();
  if (!contents) {
    return nullptr;
  }

  auto* manager = ContentInfoBarManager::FromWebContents(contents);
  if (!manager) {
    return nullptr;
  }
  if (auto* added_infobar = manager->AddInfoBar(CreateInfoBarForSpec(
          it->second, contents, std::move(params), *this))) {
    static_cast<RegistryInfoBarDelegate*>(added_infobar->delegate())
        ->set_shown();
    base::UmaHistogramSparse("InfoBar.Centralized.Show", identifier);
    return added_infobar;
  }
  return nullptr;
}

infobars::InfoBar* BrowserInfoBarManager::Replace(
    infobars::InfoBar* old_infobar,
    infobars::InfoBarDelegate::InfoBarIdentifier identifier,
    InfoBarShowParams params) {
  auto it = registered_specs_.find(identifier);
  if (it == registered_specs_.end()) {
    return nullptr;
  }
  CHECK(old_infobar);
  CHECK(params.scope.value_or(it->second.scope()) == InfoBarScope::kTab);

  infobars::InfoBarManager* manager = old_infobar->owner();
  if (!manager) {
    return nullptr;
  }

  content::WebContents* contents =
      ContentInfoBarManager::WebContentsFromInfoBar(old_infobar);
  if (!contents) {
    return nullptr;
  }

  static_cast<RegistryInfoBarDelegate*>(old_infobar->delegate())
      ->suppress_result();
  if (auto* replaced_infobar = manager->ReplaceInfoBar(
          old_infobar, CreateInfoBarForSpec(it->second, contents,
                                            std::move(params), *this))) {
    static_cast<RegistryInfoBarDelegate*>(replaced_infobar->delegate())
        ->set_shown();
    return replaced_infobar;
  }
  return nullptr;
}

bool BrowserInfoBarManager::ShowGlobally(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  return ShowGlobally(identifier, InfoBarShowParams());
}

bool BrowserInfoBarManager::ShowGlobally(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier,
    InfoBarShowParams params) {
  CHECK(!params.scope.has_value());
  auto it = registered_specs_.find(identifier);
  if (it == registered_specs_.end()) {
    return false;
  }
  CHECK(it->second.scope() == InfoBarScope::kGlobal);

  if (active_global_infobars_.contains(identifier)) {
    return false;
  }

  GlobalInfoBarContext& context = active_global_infobars_[identifier] =
      GlobalInfoBarContext{.spec = it->second, .params = std::move(params)};

  bool added_any_infobars = false;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [this, &context, identifier,
       &added_any_infobars](BrowserWindowInterface* browser) {
        if (context.spec.browser_filter() &&
            !context.spec.browser_filter().Run(browser)) {
          return true;
        }
        tabs::TabInterface* active_tab = browser->GetActiveTabInterface();
        content::WebContents* active_contents =
            active_tab ? active_tab->GetContents() : nullptr;
        if (active_contents) {
          auto* manager =
              ContentInfoBarManager::FromWebContents(active_contents);
          if (manager) {
            auto infobar = CreateInfoBarForSpec(context.spec, active_contents,
                                                context.params, *this);
            auto* added_infobar = manager->AddInfoBar(std::move(infobar));
            if (added_infobar) {
              active_global_infobars_[identifier].active_instances[manager] =
                  added_infobar;
              added_any_infobars = true;
              if (!infobar_manager_observations_.IsObservingSource(manager)) {
                infobar_manager_observations_.AddObservation(manager);
              }
            }
          }
        }
        return true;
      });

  if (added_any_infobars) {
    base::UmaHistogramSparse("InfoBar.Centralized.Show", identifier);
  } else {
    // A false return must mean nothing is showing and nothing will appear
    // later, so drop the armed context.
    active_global_infobars_.erase(identifier);
  }
  return added_any_infobars;
}

void BrowserInfoBarManager::Hide(
    content::WebContents* web_contents,
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  auto it = registered_specs_.find(identifier);
  if (it == registered_specs_.end()) {
    return;
  }
  CHECK(web_contents);

  auto* manager = ContentInfoBarManager::FromWebContents(web_contents);
  if (!manager) {
    return;
  }

  for (infobars::InfoBar* infobar : manager->infobars()) {
    if (infobar->delegate()->GetIdentifier() != identifier ||
        IsTrackedGlobalInstance(infobar)) {
      continue;
    }
    RemoveInfoBarWithoutResult(manager, infobar);
    break;
  }
}

void BrowserInfoBarManager::Hide(infobars::InfoBar* infobar) {
  CHECK(infobar);
  infobars::InfoBarManager* owner = infobar->owner();
  if (!owner) {
    return;
  }
  RemoveInfoBarWithoutResult(owner, infobar);
}

void BrowserInfoBarManager::Hide(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  auto it = registered_specs_.find(identifier);
  if (it == registered_specs_.end()) {
    return;
  }

  const InfoBarSpec& spec = it->second;

  if (spec.scope() == InfoBarScope::kTab) {
    content::WebContents* active_contents = GetActiveWebContents();
    if (!active_contents) {
      return;
    }
    Hide(active_contents, identifier);
  } else if (spec.scope() == InfoBarScope::kGlobal) {
    auto active_it = active_global_infobars_.find(identifier);
    if (active_it != active_global_infobars_.end()) {
      auto& manager_map = active_it->second.active_instances;
      while (!manager_map.empty()) {
        auto map_it = manager_map.begin();
        infobars::InfoBarManager* manager = map_it->first;
        infobars::InfoBar* infobar = map_it->second;

        manager_map.erase(
            map_it);  // Erase first to signal programmatic removal.
        RemoveInfoBarWithoutResult(manager, infobar);
      }
      active_global_infobars_.erase(active_it);
    }
  }
}

void BrowserInfoBarManager::OnBrowserCreated(BrowserWindowInterface* browser) {
  active_tab_subscriptions_[browser] =
      browser->RegisterActiveTabDidChange(base::BindRepeating(
          &BrowserInfoBarManager::OnActiveTabChanged, base::Unretained(this)));
  OnActiveTabChanged(browser);
}

void BrowserInfoBarManager::OnBrowserClosed(BrowserWindowInterface* browser) {
  active_tab_subscriptions_.erase(browser);
  last_active_managers_.erase(browser);
}

void BrowserInfoBarManager::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                             bool animate) {
  // Only tracked instances matter here. Everything in
  // `active_global_infobars_` was put there by ShowGlobally(), which only
  // accepts global specs, so membership is the scope check.
  const infobars::InfoBarDelegate::InfoBarIdentifier identifier =
      infobar->delegate()->GetIdentifier();
  auto it = active_global_infobars_.find(identifier);
  if (it == active_global_infobars_.end()) {
    return;
  }

  auto& instances = it->second.active_instances;
  const auto instance = std::ranges::find_if(
      instances,
      [infobar](const auto& entry) { return entry.second == infobar; });
  if (instance == instances.end()) {
    return;
  }
  instances.erase(instance);

  // Use the delegate, not the owner: when called from ~InfoBarManager(), the
  // owner's WebContentsObserver base is already destroyed.
  content::WebContents* web_contents =
      static_cast<RegistryInfoBarDelegate*>(infobar->delegate())
          ->web_contents();
  tabs::TabInterface* tab =
      web_contents ? tabs::TabInterface::MaybeGetFromContents(web_contents)
                   : nullptr;
  BrowserWindowInterface* browser =
      tab ? tab->GetBrowserWindowInterface() : nullptr;
  if (!browser) {
    return;
  }
  // An individual tab can go away while its window stays open, and during fast
  // shutdown its renderer may already be gone, so neither `closing_all()` nor
  // `IsDeleteScheduled()` is sufficient on its own to detect teardown.
  const bool is_tearing_down =
      browser->GetTabStripModel()->closing_all() ||
      browser->IsDeleteScheduled() || web_contents->IsBeingDestroyed() ||
      !web_contents->GetPrimaryMainFrame()->IsRenderFrameLive();
  if (!is_tearing_down) {
    static_cast<RegistryInfoBarDelegate*>(infobar->delegate())
        ->set_shown(std::exchange(it->second.pending_result, std::nullopt));
    Hide(identifier);
    return;
  }
  // The window or tab's renderer is closing/gone, not the infobar, which stays
  // up in the other browsers. This instance must not report an outcome for a
  // logical infobar the user can still see; whichever instance goes last
  // reports for it.
  if (instances.empty()) {
    static_cast<RegistryInfoBarDelegate*>(infobar->delegate())
        ->set_shown(std::exchange(it->second.pending_result, std::nullopt));
  }
}

void BrowserInfoBarManager::OnManagerWillBeDestroyed(
    infobars::InfoBarManager* manager) {
  infobar_manager_observations_.RemoveObservation(manager);

  for (auto& [identifier, context] : active_global_infobars_) {
    context.active_instances.erase(manager);
  }

  for (auto it = last_active_managers_.begin();
       it != last_active_managers_.end();) {
    if (it->second == manager) {
      it = last_active_managers_.erase(it);
    } else {
      ++it;
    }
  }
}

void BrowserInfoBarManager::OnActiveTabChanged(
    BrowserWindowInterface* browser) {
  tabs::TabInterface* active_tab = browser->GetActiveTabInterface();
  content::WebContents* active_contents =
      active_tab ? active_tab->GetContents() : nullptr;
  infobars::InfoBarManager* new_manager =
      active_contents ? ContentInfoBarManager::FromWebContents(active_contents)
                      : nullptr;

  infobars::InfoBarManager* old_manager = last_active_managers_[browser];

  if (old_manager == new_manager) {
    return;
  }

  if (old_manager) {
    for (auto& [identifier, context] : active_global_infobars_) {
      auto& manager_map = context.active_instances;
      auto it = manager_map.find(old_manager);
      if (it != manager_map.end()) {
        infobars::InfoBar* infobar = it->second;
        manager_map.erase(it);  // Erase first to signal programmatic removal.
        RemoveInfoBarWithoutResult(old_manager, infobar);
      }
    }
  }

  if (new_manager) {
    for (auto& [identifier, context] : active_global_infobars_) {
      if (context.spec.browser_filter() &&
          !context.spec.browser_filter().Run(browser)) {
        continue;
      }
      // An interaction callback switched away and back; keep its instance.
      auto existing = std::ranges::find_if(
          new_manager->infobars(), [identifier](infobars::InfoBar* ib) {
            return ib->delegate()->GetIdentifier() == identifier &&
                   static_cast<RegistryInfoBarDelegate*>(ib->delegate())
                       ->in_interaction();
          });
      if (existing != new_manager->infobars().end()) {
        static_cast<RegistryInfoBarDelegate*>((*existing)->delegate())
            ->set_close_after_interaction(false);
        context.active_instances[new_manager] = *existing;
        continue;
      }
      auto infobar = CreateInfoBarForSpec(context.spec, active_contents,
                                          context.params, *this);
      auto* added_infobar = new_manager->AddInfoBar(std::move(infobar));
      if (added_infobar) {
        context.active_instances[new_manager] = added_infobar;
        if (!infobar_manager_observations_.IsObservingSource(new_manager)) {
          infobar_manager_observations_.AddObservation(new_manager);
        }
      }
    }
  }

  last_active_managers_[browser] = new_manager;
}

bool BrowserInfoBarManager::IsTrackedGlobalInstance(
    infobars::InfoBar* infobar) const {
  auto it = active_global_infobars_.find(infobar->delegate()->GetIdentifier());
  if (it == active_global_infobars_.end()) {
    return false;
  }
  for (const auto& [manager, tracked] : it->second.active_instances) {
    if (tracked == infobar) {
      return true;
    }
  }
  return false;
}

}  // namespace infobars
