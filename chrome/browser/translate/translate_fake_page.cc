// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_fake_page.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "url/gurl.h"

FakePageImpl::FakePageImpl()
    : called_translate_(false), called_revert_translation_(false) {}
FakePageImpl::~FakePageImpl() {}

mojo::PendingRemote<translate::mojom::Page>
FakePageImpl::BindToNewPageRemote() {
  receiver_.reset();
  translate_callback_pending_.Reset();
  return receiver_.BindNewPipeAndPassRemote();
}

// translate::mojom::Page implementation.
void FakePageImpl::Translate(const std::string& translate_script,
                             const std::string& source_lang,
                             const std::string& target_lang,
                             TranslateCallback callback) {
  // Ensure pending callback gets called.
  if (translate_callback_pending_) {
    std::move(translate_callback_pending_)
        .Run(true, "", "", translate::TranslateErrors::NONE);
  }

  called_translate_ = true;
  source_lang_ = source_lang;
  target_lang_ = target_lang;

  translate_callback_pending_ = std::move(callback);
}

void FakePageImpl::RevertTranslation() {
  called_revert_translation_ = true;
}

void FakePageImpl::PageTranslated(bool cancelled,
                                  const std::string& source_lang,
                                  const std::string& target_lang,
                                  translate::TranslateErrors::Type error) {
  std::move(translate_callback_pending_)
      .Run(cancelled, source_lang, target_lang, error);
}
