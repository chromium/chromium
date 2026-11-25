// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_PAGE_SELECTION_H_
#define CHROME_BROWSER_SELECTION_PAGE_SELECTION_H_

#include <string>

#include "ui/base/interaction/safe_castable.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace selection {

// Tab-scoped. The page's text selection as seen by providers (prompt
// endpoints, suggestion tools, entry points that invoke a model). Providers
// read the selection through this interface instead of calling Chrome's
// selection context code directly.
//
// Derives from `ui::SafeCastable` so an implementation can offer its own
// `From()` through `AsA<>()` while sharing this class's user data key.
class PageSelection : public ui::SafeCastable {
 public:
  DECLARE_USER_DATA(PageSelection);

  static PageSelection* From(tabs::TabInterface* tab);

  ~PageSelection() override = default;

  // The page's selected text. Empty means none. Either the automatic
  // selection, which is whitespace-trimmed, bounded in length, and empty for
  // editable fields; or text an entry point sent explicitly (e.g. the context
  // menu), which is not length-bounded.
  virtual const std::u16string& GetText() const = 0;
};

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_PAGE_SELECTION_H_
