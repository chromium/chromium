// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_SPEC_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_SPEC_H_

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}

namespace infobars {

enum class InfoBarScope {
  // The infobar will only show for the web contents specified.
  kTab,
  // The infobar will show for all the tabs/web contents across all browsers.
  kGlobal,
};

// Terminal outcome of a shown infobar. Reported exactly once per logical
// infobar; programmatic removals are not reported.
enum class InfoBarResult {
  // The user pressed the OK button.
  kAccepted,
  // The user pressed the Cancel button.
  kCancelled,
  // The user closed the infobar.
  kDismissed,
  // Went away without the user touching it, e.g. the tab was closed.
  kIgnored,
  // The only user interaction was a link click.
  kLinkClicked,
};

// InfoBarSpec defines an InfoBar's appearance and behavior.
class InfoBarSpec {
 public:
  // Runs when the user presses a button or dismisses the infobar. The
  // infobar is torn down right after the call, so an action callback must
  // not destroy it or close the tab synchronously. Use Hide() or the
  // result callback for work that has to happen after teardown.
  using ActionCallback = base::RepeatingCallback<void(content::WebContents*)>;
  using SubstitutionsCallback =
      base::RepeatingCallback<std::vector<MessageSubstitution>(
          content::WebContents*)>;
  // Called with the substitution index when an inline link is clicked.
  // Returns true to close the infobar.
  using InlineLinkCallback = base::RepeatingCallback<
      bool(content::WebContents*, size_t, WindowOpenDisposition)>;
  // Reports the terminal outcome. The WebContents may already be gone by
  // then, in which case it is null.
  using ResultCallback =
      base::RepeatingCallback<void(content::WebContents*, InfoBarResult)>;
  // Returns true if the infobar may be shown in the given browser. Only
  // consulted for InfoBarScope::kGlobal.
  using BrowserFilter = base::RepeatingCallback<bool(BrowserWindowInterface*)>;

  class Builder;

  InfoBarSpec();
  InfoBarSpec(const InfoBarSpec&);
  InfoBarSpec(InfoBarSpec&&);
  ~InfoBarSpec();
  InfoBarSpec& operator=(const InfoBarSpec&);

  infobars::InfoBarDelegate::InfoBarIdentifier identifier() const {
    return identifier_;
  }
  const std::u16string& message_text() const { return message_text_; }
  const std::u16string& message_text_template() const {
    return message_text_template_;
  }
  const SubstitutionsCallback& substitutions_callback() const {
    return substitutions_callback_;
  }
  const InlineLinkCallback& inline_link_callback() const {
    return inline_link_callback_;
  }
  const std::u16string& link_text() const { return link_text_; }
  const GURL& link_navigation_url() const { return link_navigation_url_; }
  InfoBarDelegate::InfobarPriority priority() const { return priority_; }
  InfoBarScope scope() const { return scope_; }
  const gfx::VectorIcon* icon() const { return icon_; }
  const gfx::VectorIcon* dark_mode_icon() const { return dark_mode_icon_; }
  int icon_id() const { return icon_id_; }
  bool expire_on_navigation() const { return expire_on_navigation_; }
  bool should_hide_in_fullscreen() const { return should_hide_in_fullscreen_; }
  bool should_animate() const { return should_animate_; }
  bool is_closeable() const { return is_closeable_; }

  const std::u16string& ok_button_label() const { return ok_button_label_; }
  const ActionCallback& ok_button_callback() const {
    return ok_button_callback_;
  }
  const std::u16string& cancel_button_label() const {
    return cancel_button_label_;
  }
  const ActionCallback& cancel_button_callback() const {
    return cancel_button_callback_;
  }
  const ActionCallback& dismiss_callback() const { return dismiss_callback_; }
  const ResultCallback& result_callback() const { return result_callback_; }
  const BrowserFilter& browser_filter() const { return browser_filter_; }

 private:
  friend class Builder;
  friend class BrowserInfoBarManager;

  infobars::InfoBarDelegate::InfoBarIdentifier identifier_ =
      infobars::InfoBarDelegate::INVALID;
  std::u16string message_text_;
  std::u16string message_text_template_;
  SubstitutionsCallback substitutions_callback_;
  InlineLinkCallback inline_link_callback_;
  std::u16string link_text_;
  GURL link_navigation_url_;
  InfoBarDelegate::InfobarPriority priority_ =
      InfoBarDelegate::InfobarPriority::kDefault;
  InfoBarScope scope_ = InfoBarScope::kTab;
  raw_ptr<const gfx::VectorIcon> icon_ = nullptr;
  raw_ptr<const gfx::VectorIcon> dark_mode_icon_ = nullptr;
  int icon_id_ = 0;
  bool expire_on_navigation_ = true;
  bool should_hide_in_fullscreen_ = false;
  bool should_animate_ = true;
  bool is_closeable_ = true;

  std::u16string ok_button_label_;
  ActionCallback ok_button_callback_;
  std::u16string cancel_button_label_;
  ActionCallback cancel_button_callback_;
  ActionCallback dismiss_callback_;
  ResultCallback result_callback_;
  BrowserFilter browser_filter_;
};

// Per-show overrides for values only known at show time. Anything set here
// wins over the registered InfoBarSpec for that one instance.
struct InfoBarShowParams {
  InfoBarShowParams();
  InfoBarShowParams(InfoBarShowParams&&);
  InfoBarShowParams& operator=(InfoBarShowParams&&);
  InfoBarShowParams(const InfoBarShowParams&) = delete;
  InfoBarShowParams& operator=(const InfoBarShowParams&) = delete;
  ~InfoBarShowParams();

  // Overrides the spec's message text and suppresses its template.
  std::optional<std::u16string> message_text;

  // Substitutions for the spec's message template, computed by the caller.
  std::optional<std::vector<MessageSubstitution>> substitutions;

  // Overrides the spec's link text; empty suppresses the link.
  std::optional<std::u16string> link_text;

  // Overrides the spec's scope in Show().
  //
  // Limitation: while the override instance occupies a tab, an armed global
  // instance is deduplicated away there and only reappears on the next
  // active-tab change.
  std::optional<InfoBarScope> scope;

  // Override the spec's callbacks when non-null.
  InfoBarSpec::ActionCallback ok_button_callback;
  InfoBarSpec::ActionCallback cancel_button_callback;
  InfoBarSpec::InlineLinkCallback inline_link_callback;
  InfoBarSpec::ResultCallback result_callback;
};

class InfoBarSpec::Builder {
 public:
  explicit Builder(infobars::InfoBarDelegate::InfoBarIdentifier identifier);
  ~Builder();
  Builder(const Builder&) = delete;
  Builder& operator=(const Builder&) = delete;

  Builder& SetMessageText(std::u16string message_text);
  Builder& SetMessageTextTemplate(std::u16string message_text_template);
  Builder& SetSubstitutionsCallback(SubstitutionsCallback callback);
  Builder& SetInlineLinkCallback(InlineLinkCallback callback);
  Builder& SetLinkText(std::u16string link_text);
  Builder& SetLinkNavigationUrl(GURL gurl);
  Builder& SetIcon(const gfx::VectorIcon& icon);
  // Shown instead of the SetIcon() icon when the infobar is in dark mode.
  Builder& SetDarkModeIcon(const gfx::VectorIcon& icon);
  Builder& SetIconId(int icon_id);

  Builder& SetScope(InfoBarScope scope);
  Builder& SetPriority(InfoBarDelegate::InfobarPriority priority);
  Builder& SetExpireOnNavigation(bool expire_on_navigation);
  Builder& SetShouldHideInFullscreen(bool should_hide_in_fullscreen);
  Builder& SetShouldAnimate(bool should_animate);
  Builder& SetIsCloseable(bool is_closeable);

  Builder& AddOkButton(const std::u16string& label,
                       InfoBarSpec::ActionCallback callback);
  Builder& AddCancelButton(const std::u16string& label,
                           InfoBarSpec::ActionCallback callback);
  Builder& SetDismissAction(InfoBarSpec::ActionCallback callback);
  Builder& SetResultCallback(InfoBarSpec::ResultCallback callback);
  Builder& SetBrowserFilter(InfoBarSpec::BrowserFilter filter);

  InfoBarSpec Build();

 private:
  InfoBarSpec spec_;
};

}  // namespace infobars

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_SPEC_H_
