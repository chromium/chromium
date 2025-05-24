// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/web_contents_tags.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/task_manager/providers/web_contents/devtools_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/no_state_prefetch_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/printing_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/tab_contents_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/tool_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/web_app_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tags_manager.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/task_manager/providers/web_contents/background_contents_tag.h"
#endif

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "chrome/browser/task_manager/providers/web_contents/guest_tag.h"
#include "components/guest_view/browser/guest_view_base.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/task_manager/providers/web_contents/extension_tag.h"
#include "extensions/browser/process_manager.h"       // nogncheck
#include "extensions/browser/view_type_utils.h"       // nogncheck
#include "extensions/common/mojom/view_type.mojom.h"  // nogncheck
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS_CORE)

namespace task_manager {

namespace {

// Adds the |tag| to |contents|. It also adds the |tag| to the
// |WebContentsTagsManager|.
// Note: This will fail if |contents| is already tagged by |tag|.
void TagWebContents(content::WebContents* contents,
                    std::unique_ptr<WebContentsTag> tag,
                    const void* tag_key) {
  DCHECK(contents);
  DCHECK(tag);
  DCHECK(WebContentsTag::FromWebContents(contents) == nullptr);
  WebContentsTag* tag_ptr = tag.get();
  contents->SetUserData(tag_key, std::move(tag));
  WebContentsTagsManager::GetInstance()->AddTag(tag_ptr);
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
bool IsExtensionWebContents(content::WebContents* contents) {
  DCHECK(contents);

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  if (!base::FeatureList::IsEnabled(features::kGuestViewMPArch) &&
      guest_view::GuestViewBase::IsGuest(contents)) {
    return false;
  }
#endif

  extensions::mojom::ViewType view_type = extensions::GetViewType(contents);
  return (view_type != extensions::mojom::ViewType::kInvalid &&
          view_type != extensions::mojom::ViewType::kTabContents &&
          view_type != extensions::mojom::ViewType::kBackgroundContents &&
          view_type != extensions::mojom::ViewType::kDeveloperTools);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

}  // namespace

#if !BUILDFLAG(IS_ANDROID)
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
#endif

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

#if BUILDFLAG(ENABLE_GUEST_VIEW)
// static
void WebContentsTags::CreateForGuestContents(
    content::WebContents* web_contents) {
  // TODO(crbug.com/40202416): Support the MPArch GuestView implementation in
  // task manager.
  CHECK(!base::FeatureList::IsEnabled(features::kGuestViewMPArch));
  CHECK(guest_view::GuestViewBase::IsGuest(web_contents));
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents, base::WrapUnique(new GuestTag(web_contents)),
                   WebContentsTag::kTagKey);
  }
}
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
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
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

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
