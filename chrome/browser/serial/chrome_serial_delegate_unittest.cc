// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/chrome_serial_delegate.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/unguessable_token.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

class ChromeSerialDelegateStoragePartitionTest
    : public ChromeRenderViewHostTestHarness {
 public:
  std::unique_ptr<content::WebContents> CreateGuestPartitionWebContents(
      const GURL& url) {
    const content::StoragePartitionConfig kGuestConfig =
        content::StoragePartitionConfig::Create(
            profile(), "test_partition", "guest_partition", /*in_memory=*/true);
    scoped_refptr<content::SiteInstance> guest_instance =
        content::SiteInstance::CreateForGuest(profile(), kGuestConfig);
    std::unique_ptr<content::WebContents> guest_contents =
        content::WebContentsTester::CreateTestWebContents(profile(),
                                                          guest_instance);
    content::WebContentsTester::For(guest_contents.get())
        ->NavigateAndCommit(url);
    return guest_contents;
  }
};

TEST_F(ChromeSerialDelegateStoragePartitionTest,
       SerialBlocksPermissionAcrossStoragePartition) {
  const GURL kGuestUrl("https://example.com/");
  const url::Origin kOrigin = url::Origin::Create(kGuestUrl);

  std::unique_ptr<content::WebContents> guest =
      CreateGuestPartitionWebContents(kGuestUrl);
  content::RenderFrameHost* rfh = guest->GetPrimaryMainFrame();

  ASSERT_NE(rfh->GetStoragePartition(),
            rfh->GetBrowserContext()->GetDefaultStoragePartition());
  ASSERT_TRUE(rfh->GetLastCommittedURL().SchemeIsHTTPOrHTTPS());
  ASSERT_EQ(kOrigin, rfh->GetLastCommittedOrigin());

  // Serial should block.
  ChromeSerialDelegate serial_delegate;

  EXPECT_FALSE(serial_delegate.CanRequestPortPermission(rfh))
      << "Serial should block permission requests from non-default-partition "
         "HTTPS frames";

  device::mojom::SerialPortInfo port;
  port.token = base::UnguessableToken::Create();
  port.path = base::FilePath(FILE_PATH_LITERAL("/dev/ttyPOC0"));
  port.display_name = "POC Port";

  SerialChooserContext* chooser_context =
      SerialChooserContextFactory::GetForProfile(profile());
  ASSERT_TRUE(chooser_context);
  chooser_context->GrantPortPermission(kOrigin, port);

  EXPECT_FALSE(serial_delegate.HasPortPermission(rfh, port))
      << "Serial should block profile-wide grants leaking into "
         "non-default-partition frames";
}

}  // namespace
