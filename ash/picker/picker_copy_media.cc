// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_copy_media.h"

#include <iterator>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "ash/constants/notifier_catalogs.h"
#include "ash/picker/picker_rich_media.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "base/check_deref.h"
#include "base/functional/overloaded.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr char kPickerCopyToClipboardToastId[] = "picker_copy_to_clipboard";

struct HtmlAttr {
  // `name` has to be a compile-time constant.
  const char* name;
  std::string value;
};

std::string ImageHtmlAttrsToStr(std::vector<HtmlAttr> attrs) {
  std::vector<std::string> attrs_as_strings;
  attrs_as_strings.reserve(attrs.size());
  base::ranges::transform(
      std::move(attrs), std::back_inserter(attrs_as_strings),
      [](HtmlAttr attr) {
        return base::StringPrintf(R"(%s="%s")", attr.name, attr.value.c_str());
      });

  return base::StringPrintf(
      R"(<img %s/>)",
      base::JoinString(std::move(attrs_as_strings), " ").c_str());
}

std::string BuildImageHtml(const PickerImageMedia& image) {
  std::vector<HtmlAttr> attrs;
  // GURL::specs are always canonicalised and escaped, so this cannot result in
  // an XSS.
  attrs.emplace_back("src", image.url.spec());
  // Referrer-Policy is used to prevent the website from getting information
  // about where the GIFs are being used.
  attrs.emplace_back("referrerpolicy", "no-referrer");
  if (!image.content_description.empty()) {
    attrs.emplace_back(
        "alt",
        base::EscapeForHTML(base::UTF16ToUTF8(image.content_description)));
  }
  if (image.dimensions.has_value()) {
    attrs.emplace_back("width",
                       base::NumberToString(image.dimensions->width()));
    attrs.emplace_back("height",
                       base::NumberToString(image.dimensions->height()));
  }
  return ImageHtmlAttrsToStr(std::move(attrs));
}

}  // namespace

std::unique_ptr<ui::ClipboardData> ClipboardDataFromMedia(
    const PickerRichMedia& media,
    const PickerClipboardDataOptions& options) {
  auto data = std::make_unique<ui::ClipboardData>();
  std::visit(base::Overloaded{
                 [&data](const PickerTextMedia& media) {
                   data->set_text(base::UTF16ToUTF8(media.text));
                 },
                 [&data](const PickerImageMedia& media) {
                   data->set_markup_data(BuildImageHtml(media));
                 },
                 [&data, &options](const PickerLinkMedia& media) {
                   std::string escaped_spec =
                       base::EscapeForHTML(media.url.spec());
                   std::string escaped_title = base::EscapeForHTML(media.title);
                   data->set_text(media.url.spec());
                   if (options.links_should_use_title) {
                     data->set_markup_data(
                         base::StrCat({"<a href=\"", escaped_spec, "\">",
                                       escaped_title, "</a>"}));
                   } else {
                     data->set_markup_data(base::StrCat(
                         {"<a title=\"", escaped_title, "\" href=\"",
                          escaped_spec, "\">", escaped_spec, "</a>"}));
                   }
                 },
                 [&data](const PickerLocalFileMedia& media) {
                   data->set_filenames(
                       {ui::FileInfo(media.path, /*display_name=*/{})});
                 },
             },
             media);
  return data;
}

void CopyMediaToClipboard(const PickerRichMedia& media) {
  CHECK_DEREF(ui::ClipboardNonBacked::GetForCurrentThread())
      .WriteClipboardData(ClipboardDataFromMedia(
          media, PickerClipboardDataOptions{.links_should_use_title = false}));

  // Show a toast to inform the user about the copy.
  // TODO: b/322928125 - Use dedicated toast catalog name.
  // TODO: b/322928125 - Finalize string.
  ToastManager::Get()->Show(ToastData(
      kPickerCopyToClipboardToastId,
      ToastCatalogName::kCopyGifToClipboardAction, u"Copied to clipboard"));
}

}  // namespace ash
