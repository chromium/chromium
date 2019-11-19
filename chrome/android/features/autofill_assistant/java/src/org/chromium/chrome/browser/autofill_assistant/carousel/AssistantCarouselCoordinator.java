// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import android.support.v7.widget.RecyclerView;

/**
 * A generic interface to represent a carousel coordinator to expose its view to other components.
 */
public interface AssistantCarouselCoordinator {
    /** Return the view associated to this carousel. */
    RecyclerView getView();
}