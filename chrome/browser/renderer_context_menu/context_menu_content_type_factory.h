// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_FACTORY_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "content/public/common/context_menu_params.h"

class ContextMenuContentType;

namespace content {
class WebContents;
}

class ContextMenuContentTypeFactory {
 public:
  static std::unique_ptr<ContextMenuContentType> Create(
      content::WebContents* web_contents,
      const content::ContextMenuParams& params);

 private:
  ContextMenuContentTypeFactory();
  virtual ~ContextMenuContentTypeFactory();

  static std::unique_ptr<ContextMenuContentType> CreateInternal(
      content::WebContents* web_contents,
      const content::ContextMenuParams& params);

  DISALLOW_COPY_AND_ASSIGN(ContextMenuContentTypeFactory);
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_FACTORY_H_
