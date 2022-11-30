// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_TESTING_WIDGETS_H_
#define CHROME_BROWSER_ASH_BOREALIS_TESTING_WIDGETS_H_

#include <string>
#include "ash/test/test_widget_builder.h"

namespace borealis {

// Creates and displays a widget with the given |name|.
std::unique_ptr<views::Widget> CreateFakeWidget(std::string name,
                                                bool fullscreen = false);

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_TESTING_WIDGETS_H_
