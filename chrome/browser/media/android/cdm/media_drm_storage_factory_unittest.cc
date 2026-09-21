// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/cdm/media_drm_storage_factory.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "media/mojo/mojom/media_drm_storage.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using MediaDrmStorageFactoryTest = ChromeRenderViewHostTestHarness;

TEST_F(MediaDrmStorageFactoryTest, RejectsOffTheRecordProfile) {
  const GURL kTestOrigin("https://example.com");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             kTestOrigin);
  mojo::Remote<media::mojom::MediaDrmStorage> regular_storage;
  CreateMediaDrmStorage(main_rfh(),
                        regular_storage.BindNewPipeAndPassReceiver());
  regular_storage.FlushForTesting();
  EXPECT_TRUE(regular_storage.is_connected());

  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  std::unique_ptr<content::WebContents> otr_web_contents =
      content::WebContentsTester::CreateTestWebContents(otr_profile, nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      otr_web_contents.get(), kTestOrigin);
  mojo::Remote<media::mojom::MediaDrmStorage> otr_storage;
  CreateMediaDrmStorage(otr_web_contents->GetPrimaryMainFrame(),
                        otr_storage.BindNewPipeAndPassReceiver());
  otr_storage.FlushForTesting();
  EXPECT_FALSE(otr_storage.is_connected());
}
