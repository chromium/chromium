// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_INSPECTABLE_VIEWS_FINDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_INSPECTABLE_VIEWS_FINDER_H_

#include <vector>

#include "base/macros.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "extensions/common/view_type.h"

class Profile;
class GURL;

namespace extensions {
class Extension;
class ProcessManager;

namespace api {
namespace developer_private {
struct ExtensionView;
}
}

// Finds inspectable views for the extensions, and returns them as represented
// by the developerPrivate API structure and schema compiler.
class InspectableViewsFinder {
 public:
  using View = api::developer_private::ExtensionView;
  using ViewList = std::vector<View>;

  explicit InspectableViewsFinder(Profile* profile);
  ~InspectableViewsFinder();

  // Construct a view from the given parameters.
  static View ConstructView(const GURL& url,
                            int render_process_id,
                            int render_view_id,
                            bool incognito,
                            bool is_iframe,
                            api::developer_private::ViewType type);

  // Return a list of inspectable views for the given |extension|.
  ViewList GetViewsForExtension(const Extension& extension, bool is_enabled);

 private:
  // Returns all inspectable views for a given |profile|.
  void GetViewsForExtensionForProfile(const Extension& extension,
                                      Profile* profile,
                                      bool is_enabled,
                                      bool is_incognito,
                                      ViewList* result);

  // Returns all inspectable views for the extension process.
  void GetViewsForExtensionProcess(
      const Extension& extension,
      ProcessManager* process_manager,
      bool is_incognito,
      ViewList* result);

  // Returns all inspectable app views for the extension.
  void GetAppWindowViewsForExtension(const Extension& extension,
                                     ViewList* result);

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(InspectableViewsFinder);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_INSPECTABLE_VIEWS_FINDER_H_
