// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_TAB_STRIP_CONTEXT_DECORATOR_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_TAB_STRIP_CONTEXT_DECORATOR_H_

#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/contextual_tasks/public/context_decorator.h"
#include "components/visited_url_ranking/public/url_visit_util.h"

class Profile;

namespace contextual_tasks {

struct ContextualTaskContext;

// A decorator that enriches a context with information about whether a URL is
// currently open in the tab strip.
class TabStripContextDecorator : public ContextDecorator {
 public:
  // Represents a tab in the tab strip.
  struct TabInfo {
    GURL url;
    std::u16string title;
  };

  explicit TabStripContextDecorator(Profile* profile);
  ~TabStripContextDecorator() override;

  TabStripContextDecorator(const TabStripContextDecorator&) = delete;
  TabStripContextDecorator& operator=(const TabStripContextDecorator&) = delete;

  // ContextDecorator implementation:
  void DecorateContext(
      std::unique_ptr<ContextualTaskContext> context,
      ContextDecorationParams* params,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback) override;

 protected:
  // Virtual for testing.
  virtual std::vector<TabInfo> GetOpenTabUrls();

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_TAB_STRIP_CONTEXT_DECORATOR_H_
