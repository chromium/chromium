// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_INPUT_DENYLIST_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_INPUT_DENYLIST_H_

#include "base/values.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {

// Determines if assistive inputs should be enabled or disabled for a particular
// input field. A denylist is made up of a list of urls where assistive
// features should NOT show.
class AssistiveInputDenylist {
 public:
  AssistiveInputDenylist();
  ~AssistiveInputDenylist();

  // Is the url given found in the denylist?
  bool Contains(const GURL& url);
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_INPUT_DENYLIST_H_
