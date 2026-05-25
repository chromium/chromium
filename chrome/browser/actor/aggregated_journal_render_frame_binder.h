// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_RENDER_FRAME_BINDER_H_
#define CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_RENDER_FRAME_BINDER_H_

#include "chrome/browser/actor/aggregated_journal.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace actor {

// Binds `mojom::JournalClient` Mojo receivers to a `RenderFrameHost`.
//
// This helper allows an `AggregatedJournal` to listen for log entries emitted
// by a specific renderer process. Lifetime is tied to the `WebContents`,
// ensuring safe teardown upon document or tab destruction.
class AggregatedJournalRenderFrameBinder {
 public:
  AggregatedJournalRenderFrameBinder() = delete;

  // Ensures that journal entries emitted by `rfh` will be routed to `journal`.
  // Safe to call multiple times; subsequent calls for the same frame are a
  // no-op.
  static void EnsureBound(AggregatedJournal& journal,
                          content::RenderFrameHost& rfh);
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_RENDER_FRAME_BINDER_H_
