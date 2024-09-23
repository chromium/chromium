// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/web_contents_tags.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/task_manager/providers/web_contents/background_contents_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/devtools_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/extension_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/guest_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/no_state_prefetch_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/printing_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/tab_contents_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/tool_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/web_app_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tags_manager.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "printing/buildflags/buildflags.h"

namespace task_manager {

namespace {

// Adds the |tag| to |contents|. It also adds the |tag| to the
// |WebContentsTagsManager|.
// Note: This will fail if |contents| is already tagged by |tag|.
void TagWebContents(content::WebContents* contents,
                    std::unique_ptr<WebContentsTag> tag,
                    void* tag_key) {
  DCHECK(contents);
  DCHECK(tag);
  DCHECK(WebContentsTag::FromWebContents(contents) == nullptr);
  WebContentsTag* tag_ptr = tag.get();
  contents->SetUserData(tag_key, std::move(tag));
  WebContentsTagsManager::GetInstance()->AddTag(tag_ptr);
}

bool IsExtensionWebContents(content::WebContents* contents) {
  DCHECK(contents);

  if (guest_view::GuestViewBase::IsGuest(contents))
    return false;

  extensions::mojom::ViewType view_type = extensions::GetViewType(contents);
  return (view_type != extensions::mojom::ViewType::kInvalid &&
          view_type != extensions::mojom::ViewType::kTabContents &&
          view_type != extensions::mojom::ViewType::kBackgroundContents &&
          view_type != extensions::mojom::ViewType::kDeveloperTools);
}

}  // namespace

// static
void WebContentsTags::CreateForBackgroundContents(
    content::WebContents* web_contents,
    BackgroundContents* background_contents) {
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents,
                   base::WrapUnique(new BackgroundContentsTag(
                       web_contents, background_contents)),
                   WebContentsTag::kTagKey);
  }
}

// static
void WebContentsTags::CreateForDevToolsContents(
    content::WebContents* web_contents) {
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents,
                   base::WrapUnique(new DevToolsTag(web_contents)),
                   WebContentsTag::kTagKey);
  }
}

// static
void WebContentsTags::CreateForNoStatePrefetchContents(
    content::WebContents* web_contents) {
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents,
                   base::WrapUnique(new NoStatePrefetchTag(web_contents)),
                   WebContentsTag::kTagKey);
  }
}

// static
void WebContentsTags::CreateForTabContents(content::WebContents* web_contents) {
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents,
                   base::WrapUnique(new TabContentsTag(web_contents)),
                   WebContentsTag::kTagKey);
  }
}

// static
void WebContentsTags::CreateForPrintingContents(
    content::WebContents* web_contents) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents,
                   base::WrapUnique(new PrintingTag(web_contents)),
                   WebContentsTag::kTagKey);
  }
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
}

// static
void WebContentsTags::CreateForGuestContents(
    content::WebContents* web_contents) {
  CHECK(guest_view::GuestViewBase::IsGuest(web_contents));
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents, base::WrapUnique(new GuestTag(web_contents)),
                   WebContentsTag::kTagKey);
  }
}

// static
void WebContentsTags::CreateForExtension(
    content::WebContents* web_contents,
    extensions::mojom::ViewType view_type) {
  DCHECK(IsExtensionWebContents(web_contents));

  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents,
                   base::WrapUnique(new ExtensionTag(web_contents, view_type)),
                   WebContentsTag::kTagKey);
  }
}

// static
void WebContentsTags::CreateForWebApp(content::WebContents* web_contents,
                                      const webapps::AppId& app_id,
                                      const bool is_isolated_web_app) {
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents,
                   base::WrapUnique(new WebAppTag(web_contents, app_id,
                                                  is_isolated_web_app)),
                   WebContentsTag::kTagKey);
  }
}

// static
void WebContentsTags::CreateForToolContents(content::WebContents* web_contents,
                                            int tool_name) {
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents,
                   base::WrapUnique(new ToolTag(web_contents, tool_name)),
                   WebContentsTag::kTagKey);
  }
}

// static
void WebContentsTags::ClearTag(content::WebContents* web_contents) {
  // Some callers may clear the tag of a contents that is currently untagged
  // (for example, it may have previously been cleared). Doing so is a no-op.
  const WebContentsTag* tag = WebContentsTag::FromWebContents(web_contents);
  if (!tag)
    return;
  WebContentsTagsManager::GetInstance()->ClearFromProvider(tag);
  web_contents->RemoveUserData(WebContentsTag::kTagKey);
}

}  // namespace task_manager
