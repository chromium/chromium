// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DETACHED_WEB_CONTENTS_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DETACHED_WEB_CONTENTS_H_

#include "base/functional/callback_helpers.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace views {
class Widget;
}

class Profile;

// Helper class to manage an instance of content::WebContents which is not
// attached to any tab strip or visible widget. This content::WebContents is
// essentially invisible to the user.
class DetachedWebContents {
 public:
  DetachedWebContents(Profile* profile, const GURL& url);
  ~DetachedWebContents();

  content::WebContents* GetWebContents();

  // Enforces clients to destroy DetachedWebContents in order to obtain
  // underlying WebContents.
  static std::unique_ptr<content::WebContents> ReleaseWebContents(
      std::unique_ptr<DetachedWebContents> detached_web_contents);

 private:
  std::unique_ptr<content::WebContents> web_contents_;

  base::ScopedClosureRunner closure_runner_;

  std::unique_ptr<views::Widget> widget_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_DETACHED_WEB_CONTENTS_H_
