// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/quick_insert_copy_media.h"

#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/quick_insert/quick_insert_rich_media.h"
#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/strong_alias.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr char kQuickInsertCopyToClipboardToastId[] =
    "quick_insert_copy_to_clipboard";

struct HtmlAttr {
 private:
  using Name = base::StrongAlias<class NameTag, std::string_view>;

 public:
  static constexpr auto kSrc = Name("src");
  static constexpr auto kReferrerPolicy = Name("referrerpolicy");
  static constexpr auto kAlt = Name("alt");
  static constexpr auto kWidth = Name("width");
  static constexpr auto kHeight = Name("height");

  // `name` must be set to one of the values above.
  Name name;
  std::string value;
};

std::string ImageHtmlAttrsToStr(std::vector<HtmlAttr> attrs) {
  return base::StringPrintf(
      R"(<img %s/>)",
      base::JoinString(base::ToVector(attrs,
                                      [](const HtmlAttr& attr) {
                                        return base::StringPrintf(
                                            R"(%s="%s")", *attr.name,
                                            attr.value.c_str());
                                      }),
                       " ")
          .c_str());
}

std::string BuildImageHtml(const QuickInsertImageMedia& image) {
  std::vector<HtmlAttr> attrs;
  // GURL::specs are always canonicalised and escaped, so this cannot result in
  // an XSS.
  attrs.emplace_back(HtmlAttr::kSrc, image.url.spec());
  // Referrer-Policy is used to prevent the website from getting information
  // about where the GIFs are being used.
  attrs.emplace_back(HtmlAttr::kReferrerPolicy, "no-referrer");
  if (!image.content_description.empty()) {
    attrs.emplace_back(
        HtmlAttr::kAlt,
        base::EscapeForHTML(base::UTF16ToUTF8(image.content_description)));
  }
  if (image.dimensions.has_value()) {
    attrs.emplace_back(HtmlAttr::kWidth,
                       base::NumberToString(image.dimensions->width()));
    attrs.emplace_back(HtmlAttr::kHeight,
                       base::NumberToString(image.dimensions->height()));
  }
  return ImageHtmlAttrsToStr(std::move(attrs));
}

}  // namespace

std::unique_ptr<ui::ClipboardData> ClipboardDataFromMedia(
    const QuickInsertRichMedia& media,
    const QuickInsertClipboardDataOptions& options) {
  auto data = std::make_unique<ui::ClipboardData>();
  std::visit(
      absl::Overload{
          [&data](const QuickInsertTextMedia& media) {
            data->set_text(base::UTF16ToUTF8(media.text));
          },
          [&data](const QuickInsertImageMedia& media) {
            data->set_markup_data(BuildImageHtml(media));
          },
          [&data, &options](const QuickInsertLinkMedia& media) {
            std::string escaped_spec = base::EscapeForHTML(media.url.spec());
            std::string escaped_title = base::EscapeForHTML(media.title);
            data->set_text(media.url.spec());
            if (options.links_should_use_title) {
              data->set_markup_data(base::StrCat(
                  {"<a href=\"", escaped_spec, "\">", escaped_title, "</a>"}));
            } else {
              data->set_markup_data(
                  base::StrCat({"<a title=\"", escaped_title, "\" href=\"",
                                escaped_spec, "\">", escaped_spec, "</a>"}));
            }
          },
          [&data](const QuickInsertLocalFileMedia& media) {
            data->set_filenames(
                {ui::FileInfo(media.path, /*display_name=*/{})});
          },
      },
      media);
  return data;
}

void CopyMediaToClipboard(const QuickInsertRichMedia& media) {
  CHECK_DEREF(ui::ClipboardNonBacked::GetForCurrentThread())
      .WriteClipboardData(ClipboardDataFromMedia(
          media,
          QuickInsertClipboardDataOptions{.links_should_use_title = false}));

  // Show a toast to inform the user about the copy.
  // TODO: b/322928125 - Use dedicated toast catalog name.
  // TODO: b/322928125 - Finalize string.
  ToastManager::Get()->Show(ToastData(
      kQuickInsertCopyToClipboardToastId,
      ToastCatalogName::kCopyGifToClipboardAction, u"Copied to clipboard"));
}

}  // namespace ash
