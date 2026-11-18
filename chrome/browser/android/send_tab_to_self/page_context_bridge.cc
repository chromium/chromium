// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <vector>

#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/proto_conversions.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize `FromJniType()` / `ToJniType()`.
#include "chrome/android/chrome_jni_headers/PageContextBridge_jni.h"

namespace send_tab_to_self {

static void JNI_PageContextBridge_FillWebContents(
    JNIEnv* env,
    content::WebContents* web_contents,
    const std::vector<uint8_t>& page_context_bytes,
    const GURL& url) {
  if (!web_contents || page_context_bytes.empty() || !url.is_valid()) {
    return;
  }

  sync_pb::PageContext proto;
  if (!proto.ParseFromArray(page_context_bytes.data(),
                            page_context_bytes.size())) {
    return;
  }

  PageContext page_context = PageContextFromProto(proto, url);
  FillWebContents(web_contents, url::Origin::Create(url), page_context);
}

}  // namespace send_tab_to_self

DEFINE_JNI(PageContextBridge)
