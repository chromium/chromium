// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/password_manager/password_change/annotated_page_content_capturer_impl.h"
#include "chrome/browser/password_manager/password_change/password_change_page_stability_waiter.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

// static
AnnotatedPageContentCapturer::FactoryCallback&
AnnotatedPageContentCapturer::GetFactory() {
  static base::NoDestructor<FactoryCallback> factory;
  return *factory;
}

std::unique_ptr<AnnotatedPageContentCapturer>
AnnotatedPageContentCapturer::Create(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    blink::mojom::AIPageContentOptionsPtr options,
    optimization_guide::OnAIPageContentDone callback) {
  auto& factory = GetFactory();
  if (factory) {
    return factory.Run(web_contents, std::move(options), std::move(callback));
  }
  return std::make_unique<AnnotatedPageContentCapturerImpl>(
      web_contents, client, std::move(options), std::move(callback),
      base::BindRepeating(&optimization_guide::GetAIPageContent,
                          base::Unretained(web_contents)));
}
