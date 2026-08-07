// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_LOCAL_STORAGE_MIGRATION_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_LOCAL_STORAGE_MIGRATION_H_

namespace content {
class BrowserContext;
}  // namespace content

namespace glic {

// Performs a one-time migration of specified Glic local storage keys
// (BARD_EMBED_CHAT_STORAGE_KEY_V2 and WEB_EMBEDDED_CHROME_CAPABILITIES_STORAGE)
// from the Glic storage partition to the default/main storage partition.
// The migration is skipped if disabled by flags, or already completed.
// TODO(b/534807813): We should defer loading the web client until after this
// migration is complete, otherwise it will race.
void MaybeMigrateGlicLocalStorage(content::BrowserContext* browser_context);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_LOCAL_STORAGE_MIGRATION_H_
