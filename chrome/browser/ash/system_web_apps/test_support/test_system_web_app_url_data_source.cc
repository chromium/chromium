// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_url_data_source.h"

#include <utility>

#include "base/memory/ref_counted_memory.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

namespace {

constexpr char kManifestText[] =
    R"({
      "name": "Test System App",
      "display": "standalone",
      "icons": [
        {
          "src": "icon-256.png",
          "sizes": "256x256",
          "type": "image/png"
        }
      ],
      "start_url": "/pwa.html",
      "theme_color": "#00FF00"
    })";

constexpr char kPwaHtml[] =
    R"(
<html>
<head>
  <link rel="manifest" href="manifest.json">
  <style>
    body {
      background-color: white;
    }
    @media(prefers-color-scheme: dark) {
      body {
        background-color: black;
      }
    }
  </style>
  <script>
    navigator.serviceWorker.register('sw.js');
  </script>
</head>
</html>
)";

constexpr char kPage2Html[] =
    R"(
<!DOCTYPE html><title>Page 2</title>
  )";

constexpr char kSwJs[] = "globalThis.addEventListener('fetch', event => {});";

}  // namespace

void AddTestURLDataSource(const std::string& source_name,
                          content::BrowserContext* browser_context) {
  content::WebUIDataSource* data_source =
      content::WebUIDataSource::CreateAndAdd(browser_context, source_name);
  webui::EnableTrustedTypesCSP(data_source);
  data_source->AddResourcePath("icon-256.png", IDR_PRODUCT_LOGO_256);
  data_source->SetRequestFilter(
      base::BindLambdaForTesting([](const std::string& path) {
        return path == "manifest.json" || path == "pwa.html" ||
               path == "page2.html";
      }),
      base::BindLambdaForTesting(
          [](const std::string& id,
             content::WebUIDataSource::GotDataCallback callback) {
            scoped_refptr<base::RefCountedString> ref_contents(
                new base::RefCountedString);
            if (id == "manifest.json")
              ref_contents->as_string() = kManifestText;
            else if (id == "pwa.html")
              ref_contents->as_string() = kPwaHtml;
            else if (id == "sw.js")
              ref_contents->as_string() = kSwJs;
            else if (id == "page2.html")
              ref_contents->as_string() = kPage2Html;
            else
              NOTREACHED_IN_MIGRATION();

            std::move(callback).Run(ref_contents);
          }));
}

}  // namespace ash
