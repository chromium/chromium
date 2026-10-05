// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_media_list.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents.h"

DesktopMediaList::WebContentsFilter DesktopMediaList::ExcludeWebContents(
    WebContentsFilter filter,
    content::WebContents* excluded_web_contents) {
  CHECK(excluded_web_contents, base::NotFatalUntil::M161);

  return base::BindRepeating(
      [](DesktopMediaList::WebContentsFilter filter,
         base::WeakPtr<content::WebContents> excluded_web_contents,
         content::WebContents* candidate_web_contents) {
        CHECK(candidate_web_contents,
              base::NotFatalUntil::M161);  // But maybe !excluded_web_contents.
        return excluded_web_contents.get() != candidate_web_contents &&
               filter.Run(candidate_web_contents);
      },
      std::move(filter), excluded_web_contents->GetWeakPtr());
}

DesktopMediaList::Source::Source() = default;

DesktopMediaList::Source::Source(const Source& other_source) = default;

DesktopMediaList::Source::~Source() = default;
