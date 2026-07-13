// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_SPEC_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_SPEC_H_

#include <cstddef>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

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

enum class InfoBarPriority {
  kDefault,
  kHigh,
  kCriticalSecurity,
};

// InfoBarSpec defines an InfoBar's appearance and behavior.
class InfoBarSpec {
 public:
  using ActionCallback = base::RepeatingCallback<void(content::WebContents*)>;
  using SubstitutionsCallback =
      base::RepeatingCallback<std::vector<MessageSubstitution>(
          content::WebContents*)>;
  using InlineLinkCallback = base::RepeatingCallback<
      void(content::WebContents*, size_t, WindowOpenDisposition)>;

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
  InfoBarPriority priority() const { return priority_; }
  InfoBarScope scope() const { return scope_; }
  const gfx::VectorIcon* icon() const { return icon_; }
  int icon_id() const { return icon_id_; }
  bool expire_on_navigation() const { return expire_on_navigation_; }
  bool should_hide_in_fullscreen() const { return should_hide_in_fullscreen_; }

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
  InfoBarPriority priority_ = InfoBarPriority::kDefault;
  InfoBarScope scope_ = InfoBarScope::kTab;
  raw_ptr<const gfx::VectorIcon> icon_ = nullptr;
  int icon_id_ = 0;
  bool expire_on_navigation_ = true;
  bool should_hide_in_fullscreen_ = false;

  std::u16string ok_button_label_;
  ActionCallback ok_button_callback_;
  std::u16string cancel_button_label_;
  ActionCallback cancel_button_callback_;
  ActionCallback dismiss_callback_;
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
  Builder& SetIconId(int icon_id);

  Builder& SetScope(InfoBarScope scope);
  Builder& SetPriority(InfoBarPriority priority);
  Builder& SetExpireOnNavigation(bool expire_on_navigation);
  Builder& SetShouldHideInFullscreen(bool should_hide_in_fullscreen);

  Builder& AddOkButton(const std::u16string& label,
                       InfoBarSpec::ActionCallback callback);
  Builder& AddCancelButton(const std::u16string& label,
                           InfoBarSpec::ActionCallback callback);
  Builder& SetDismissAction(InfoBarSpec::ActionCallback callback);

  InfoBarSpec Build();

 private:
  InfoBarSpec spec_;
};

}  // namespace infobars

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_SPEC_H_
