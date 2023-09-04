// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_UTILS_H_
#define CHROME_BROWSER_FAVICON_FAVICON_UTILS_H_

#include "components/favicon/content/content_favicon_driver.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Image;
}  // namespace gfx

namespace favicon {

// Creates a ContentFaviconDriver and associates it with |web_contents| if none
// exists yet.
//
// This is a helper method for ContentFaviconDriver::CreateForWebContents() that
// gets KeyedService factories from the Profile linked to web_contents.
void CreateContentFaviconDriverForWebContents(
    content::WebContents* web_contents);

// Generates a monogram in a colored circle. The color is based on the hash
// of the url and the monogram is the first letter of the URL domain.
SkBitmap GenerateMonogramFavicon(GURL url, int icon_size, int circle_size);

// Retrieves the favicon from given WebContents. If contents contain a
// network error, desaturate the favicon.
gfx::Image TabFaviconFromWebContents(content::WebContents* contents);

// Returns the image to use when no favicon is available, taking dark mode
// into account if necessary.
gfx::Image GetDefaultFavicon();

// Returns the image to use when no favicon is available, taking the background
// color into account. If no background color is provided the window background
// color will be used (which is appropriate for most use cases).
ui::ImageModel GetDefaultFaviconModel(
    ui::ColorId bg_color = ui::kColorWindowBackground);

// Saves the favicon for the last committed navigation entry to the favicon
// database.
void SaveFaviconEvenIfInIncognito(content::WebContents* contents);

// Return true if the favicon for |entry| should be themified, based on URL as
// some chrome pages shouldn't be themified like apps or Password Manager.
bool ShouldThemifyFavicon(GURL url);

// Return true if the favicon for |entry| should be themified, based on both
// its visible and actual URL.
bool ShouldThemifyFaviconForEntry(content::NavigationEntry* entry);

// Recolor favicon with |alternate_color| if contrast ratio is low between
// source color and background |active_background| or
// |inactive_background|.
gfx::ImageSkia ThemeFavicon(const gfx::ImageSkia& source,
                            SkColor alternate_color,
                            SkColor active_background,
                            SkColor inactive_background);

// Recolor the favicon kGoogleGrey900 or white, based on which gives the most
// contrast against `background`.
gfx::ImageSkia ThemeMonochromeFavicon(const gfx::ImageSkia& source,
                                      SkColor background);

}  // namespace favicon

#endif  // CHROME_BROWSER_FAVICON_FAVICON_UTILS_H_
