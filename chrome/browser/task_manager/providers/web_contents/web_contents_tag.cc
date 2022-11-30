// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/web_contents_tag.h"

#include "chrome/browser/task_manager/providers/web_contents/web_contents_tags_manager.h"
#include "content/public/browser/web_contents.h"

namespace task_manager {

// static
const WebContentsTag* WebContentsTag::FromWebContents(
    const content::WebContents* contents) {
  DCHECK(contents);
  return static_cast<const WebContentsTag*>(contents->GetUserData(kTagKey));
}

WebContentsTag::WebContentsTag(content::WebContents* contents)
    : web_contents_(contents) {
  DCHECK(contents);
  // You can't tag the |contents| here. The object creation is not complete yet.
  // This will be done in the factory methods inside
  // |task_manager::WebContentsTags|.
}

WebContentsTag::~WebContentsTag() {
  WebContentsTagsManager::GetInstance()->RemoveTag(this);
}

// static
void* WebContentsTag::kTagKey = &WebContentsTag::kTagKey;

}  // namespace task_manager
