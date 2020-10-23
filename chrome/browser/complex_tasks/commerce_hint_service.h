// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPLEX_TASKS_COMMERCE_HINT_SERVICE_H_
#define CHROME_BROWSER_COMPLEX_TASKS_COMMERCE_HINT_SERVICE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/common/complex_tasks/commerce_hints.mojom.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace complex_tasks {

class CommerceHintService
    : public content::WebContentsUserData<CommerceHintService> {
 public:
  ~CommerceHintService() override;
  void BindCommerceHintObserver(
      mojo::PendingReceiver<mojom::CommerceHintObserver> receiver);
  content::WebContents* WebContents();

 private:
  explicit CommerceHintService(content::WebContents* web_contents);
  friend class content::WebContentsUserData<CommerceHintService>;

  content::WebContents* web_contents_;

  base::WeakPtrFactory<CommerceHintService> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace complex_tasks

#endif  // CHROME_BROWSER_COMPLEX_TASKS_COMMERCE_HINT_SERVICE_H_
