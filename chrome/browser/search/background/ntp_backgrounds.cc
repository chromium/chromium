// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_backgrounds.h"

#include "url/gurl.h"

std::array<GURL, kNtpBackgroundsCount> GetNtpBackgrounds() {
  // A set of whitelisted NTP background image URLs that are always considered
  // to be valid URLs that are shown to the user as part of the welcome flow.
  // These backgrounds were handpicked from the Backdrop API based on popularity
  // and those requiring minimum maintenance and translation work. This list
  // matches with chrome/browser/ui/webui/welcome/nux/ntp_background_handler.cc.
  const std::array<GURL, kNtpBackgroundsCount> kNtpBackgrounds = {{
      // Art
      GURL("https://lh5.googleusercontent.com/proxy/"
           "E60bugMrz3Jw0Ty3vD1HqfrrabnAQGlHzIJjRadV1kDS_"
           "XSE0AtWuMnjW9VPvq1YeyPJK13gZw63TQYvh2RlaZq_"
           "aQm5xskpsgWW1l67gg3mkYaZr07BQqMV47onKA=w3840-h2160-p-k-no-nd-"
           "mv"),

      // Cityscape
      GURL("https://lh4.googleusercontent.com/proxy/"
           "UOhQwfclsAK8TnXZqoTkh9szHvYOJ3auDH07hZBZeVaaRWvzGaXpaYl60MfCRuW"
           "_S57gvzBw859pj5Xl2pW_GpfG8k2GhE9LUFNKwA=w3840-h2160-p-k-no-nd-"
           "mv"),

      // Earth
      GURL("https://lh5.googleusercontent.com/proxy/"
           "xvtq6_782kBajCBr0GISHpujOb51XLKUeEOJ2lLPKh12-"
           "xNBTCtsoHT14NQcaH9l4JhatcXEMBkqgUeCWhb3XhdLnD1BiNzQ_LVydwg="
           "w3840-h2160-p-k-no-nd-mv"),

      // Geometric Shapes
      GURL("https://lh3.googleusercontent.com/proxy/"
           "FWOBAVfQYasxV3KURX1VVKem1yOC2iazWAb8csOmqCDwI1CCzAA1zCpnAxR-"
           "wL2rbfZNcRHbI5b-SZfLASmF7uhJnzrksBWougEGlkw_-4U=w3840-h2160-p-"
           "k-no-nd-mv"),

      // Landscapes
      GURL("https://lh3.googleusercontent.com/proxy/"
           "nMIspgHzTUU0GzmiadmPphBelzF2xy9-tIiejZg3VvJTITxUb-1vILxf-"
           "IsCfyl94VSn6YvHa8_PiIyR9d3rwD8ZhNdQ1C1rnblP6zy3OaI=w3840-h2160-"
           "p-k-no-nd-mv"),
  }};
  return kNtpBackgrounds;
}
