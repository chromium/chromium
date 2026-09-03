// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/browser_infobar_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
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
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace infobars {

namespace {

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
                          InfoBarShowParams params)
      : content::WebContentsObserver(contents),
        spec_(std::move(spec)),
        params_(std::move(params)) {
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
      ReportResult(*pending_result_);
    }
  }

  // Called once the infobar has actually been added.
  void set_shown() { pending_result_ = InfoBarResult::kIgnored; }

  // Keeps a manager-initiated removal from being reported as an outcome.
  void suppress_result() { pending_result_.reset(); }

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return spec_.identifier();
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

  const std::vector<MessageSubstitution>& GetMessageSubstitutions()
      const override {
    if (!substitutions_.has_value()) {
      substitutions_ = spec_.substitutions_callback()
                           ? spec_.substitutions_callback().Run(web_contents())
                           : std::vector<MessageSubstitution>();
    }
    return *substitutions_;
  }

  std::u16string GetLinkText() const override {
    return params_.link_text.value_or(spec_.link_text());
  }

  GURL GetLinkURL() const override { return spec_.link_navigation_url(); }

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
    if (!spec_.ok_button_label().empty() || spec_.ok_button_callback() ||
        params_.ok_button_callback) {
      buttons |= BUTTON_OK;
    }
    if (!spec_.cancel_button_label().empty() ||
        spec_.cancel_button_callback() || params_.cancel_button_callback) {
      buttons |= BUTTON_CANCEL;
    }
    return buttons;
  }

  std::u16string GetButtonLabel(InfoBarButton button) const override {
    if (button == BUTTON_OK && !spec_.ok_button_label().empty()) {
      return spec_.ok_button_label();
    }
    if (button == BUTTON_CANCEL && !spec_.cancel_button_label().empty()) {
      return spec_.cancel_button_label();
    }
    return ConfirmInfoBarDelegate::GetButtonLabel(button);
  }

  bool Accept() override {
    base::UmaHistogramSparse("InfoBar.Centralized.Accept", GetIdentifier());
    const InfoBarSpec::ActionCallback& callback =
        params_.ok_button_callback ? params_.ok_button_callback
                                   : spec_.ok_button_callback();
    auto* contents = web_contents();
    if (contents && callback) {
      callback.Run(contents);
    }
    ReportResult(InfoBarResult::kAccepted);
    return spec_.close_on_accept();
  }

  bool Cancel() override {
    base::UmaHistogramSparse("InfoBar.Centralized.Cancel", GetIdentifier());
    const InfoBarSpec::ActionCallback& callback =
        params_.cancel_button_callback ? params_.cancel_button_callback
                                       : spec_.cancel_button_callback();
    auto* contents = web_contents();
    if (contents && callback) {
      callback.Run(contents);
    }
    ReportResult(InfoBarResult::kCancelled);
    return true;
  }

  void InfoBarDismissed() override {
    base::UmaHistogramSparse("InfoBar.Centralized.Dismiss", GetIdentifier());
    auto* contents = web_contents();
    if (contents && spec_.dismiss_callback()) {
      spec_.dismiss_callback().Run(contents);
    }
    ReportResult(InfoBarResult::kDismissed);
  }

  bool LinkClicked(WindowOpenDisposition disposition) override {
    if (pending_result_) {
      pending_result_ = InfoBarResult::kLinkClicked;
    }
    base::UmaHistogramSparse("InfoBar.Centralized.LinkClicked",
                             GetIdentifier());
    return ConfirmInfoBarDelegate::LinkClicked(disposition);
  }

  bool InlineSubstitutionLinkClicked(
      size_t index,
      WindowOpenDisposition disposition) override {
    if (pending_result_) {
      pending_result_ = InfoBarResult::kLinkClicked;
    }
    base::UmaHistogramSparse("InfoBar.Centralized.LinkClicked",
                             GetIdentifier());
    const InfoBarSpec::InlineLinkCallback& callback =
        params_.inline_link_callback ? params_.inline_link_callback
                                     : spec_.inline_link_callback();
    if (callback) {
      return callback.Run(web_contents(), index, disposition);
    }
    return false;
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
  void ReportResult(InfoBarResult result) {
    if (!pending_result_) {
      return;
    }

    pending_result_.reset();

    const InfoBarSpec::ResultCallback& callback = params_.result_callback
                                                      ? params_.result_callback
                                                      : spec_.result_callback();
    if (callback) {
      callback.Run(web_contents(), result);
    }
  }

  InfoBarSpec spec_;
  InfoBarShowParams params_;

  // The terminal outcome still owed at destruction, cleared once an
  // interaction reports its own result.
  std::optional<InfoBarResult> pending_result_;

  // Computed once and cached so the substitutions don't change under the
  // view.
  mutable std::optional<std::vector<MessageSubstitution>> substitutions_;
};

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

// Keeps `infobar` from reporting a result when it goes away. Every infobar
// here was created by this manager, so the cast is safe.
void SuppressInfoBarResult(infobars::InfoBar* infobar) {
  static_cast<RegistryInfoBarDelegate*>(infobar->delegate())->suppress_result();
}

// Removes `infobar` without reporting a result.
void RemoveInfoBarWithoutResult(infobars::InfoBarManager* manager,
                                infobars::InfoBar* infobar) {
  SuppressInfoBarResult(infobar);
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
  if (auto* added_infobar = manager->AddInfoBar(
          CreateConfirmInfoBar(std::make_unique<RegistryInfoBarDelegate>(
              it->second, contents, std::move(params))))) {
    static_cast<RegistryInfoBarDelegate*>(added_infobar->delegate())
        ->set_shown();
    base::UmaHistogramSparse("InfoBar.Centralized.Show", identifier);
    return added_infobar;
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
            auto infobar =
                CreateConfirmInfoBar(std::make_unique<RegistryInfoBarDelegate>(
                    context.spec, active_contents, context.params));
            auto* added_infobar = manager->AddInfoBar(std::move(infobar));
            if (added_infobar) {
              static_cast<RegistryInfoBarDelegate*>(added_infobar->delegate())
                  ->set_shown();
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
  infobars::InfoBarDelegate::InfoBarIdentifier identifier =
      infobar->delegate()->GetIdentifier();

  infobars::InfoBarManager* found_manager = nullptr;
  auto it = active_global_infobars_.find(identifier);
  if (it != active_global_infobars_.end()) {
    auto& manager_map = it->second.active_instances;
    for (auto& [manager, ib] : manager_map) {
      if (ib == infobar) {
        found_manager = manager;
        break;
      }
    }
    if (found_manager) {
      manager_map.erase(found_manager);
    }
  }

  if (found_manager) {
    if (IsGlobal(identifier)) {
      content::WebContents* web_contents =
          ContentInfoBarManager::WebContentsFromInfoBar(infobar);
      if (web_contents) {
        BrowserWindowInterface* browser =
            FindBrowserWithWebContents(web_contents);
        if (browser) {
          if (browser->GetTabStripModel()->closing_all() ||
              browser->IsDeleteScheduled()) {
            // The window is closing, not the infobar, which stays up in the
            // other browsers. This instance must not report an outcome for
            // a logical infobar the user can still see; whichever instance
            // goes last reports for it.
            if (!it->second.active_instances.empty()) {
              SuppressInfoBarResult(infobar);
            }
          } else {
            Hide(identifier);
          }
        }
      }
    }
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
      auto infobar =
          CreateConfirmInfoBar(std::make_unique<RegistryInfoBarDelegate>(
              context.spec, active_contents, context.params));
      auto* added_infobar = new_manager->AddInfoBar(std::move(infobar));
      if (added_infobar) {
        static_cast<RegistryInfoBarDelegate*>(added_infobar->delegate())
            ->set_shown();
        context.active_instances[new_manager] = added_infobar;
        if (!infobar_manager_observations_.IsObservingSource(new_manager)) {
          infobar_manager_observations_.AddObservation(new_manager);
        }
      }
    }
  }

  last_active_managers_[browser] = new_manager;
}

bool BrowserInfoBarManager::IsGlobal(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  auto it = registered_specs_.find(identifier);
  return it != registered_specs_.end() &&
         it->second.scope() == InfoBarScope::kGlobal;
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

BrowserWindowInterface* BrowserInfoBarManager::FindBrowserWithWebContents(
    content::WebContents* web_contents) {
  for (const auto& [browser, subscription] : active_tab_subscriptions_) {
    for (tabs::TabInterface* tab : browser->GetAllTabInterfaces()) {
      if (tab->GetContents() == web_contents) {
        return browser;
      }
    }
  }
  return nullptr;
}

}  // namespace infobars
