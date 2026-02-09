// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_VIEWS_DEVTOOLS_FLOATY_H_
#define CHROME_BROWSER_DEVTOOLS_VIEWS_DEVTOOLS_FLOATY_H_

class Profile;

namespace gfx {
class Point;
}

namespace DevToolsFloaty {

// Creates and shows the floating window for a given `profile`. The `process_id`
// and `routing_id` refer to the inspected web page and `position` and
// `backend_node_id` refer to the node being inspected (where the node was
// clicked and what its id is).
void Show(Profile* profile,
          int process_id,
          int routing_id,
          gfx::Point position,
          int backend_node_id);

// Restores and focuses the floating window for the given backend node ID.
// `backend_node_id` refer to id of the node being inspected.
void Restore(int backend_node_id);

}  // namespace DevToolsFloaty

#endif  // CHROME_BROWSER_DEVTOOLS_VIEWS_DEVTOOLS_FLOATY_H_
