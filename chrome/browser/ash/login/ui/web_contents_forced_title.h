// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_WEB_CONTENTS_FORCED_TITLE_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_WEB_CONTENTS_FORCED_TITLE_H_

#include <string>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace ash {

// Ensures that the title of the WebContents instance this object is attached
// to is always set to the given title value.
class WebContentsForcedTitle
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebContentsForcedTitle> {
 public:
  static void CreateForWebContentsWithTitle(content::WebContents* web_contents,
                                            const std::u16string& title);

  WebContentsForcedTitle(const WebContentsForcedTitle&) = delete;
  WebContentsForcedTitle& operator=(const WebContentsForcedTitle&) = delete;

  ~WebContentsForcedTitle() override;

 private:
  friend class content::WebContentsUserData<WebContentsForcedTitle>;
  WebContentsForcedTitle(content::WebContents* web_contents,
                         const std::u16string& title);

  // content::WebContentsObserver:
  void TitleWasSet(content::NavigationEntry* entry) override;

  std::u16string title_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_WEB_CONTENTS_FORCED_TITLE_H_
