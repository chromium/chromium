// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes.images;

import org.chromium.components.content_creation.notes.models.Background;
import org.chromium.components.content_creation.notes.models.ImageBackground;
import org.chromium.components.image_fetcher.ImageFetcher;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Service in charge of making use of the ImageFetcher components to load
 * Backgrounds' images.
 */
public class ImageService {
    private final ImageFetcher mImageFetcher;

    /**
     * Constructor.
     * @param imageFetcher An ImageFetcher instance to be used by the service to retrieve remote
     *         images.
     */
    public ImageService(ImageFetcher imageFetcher) {
        this.mImageFetcher = imageFetcher;
    }

    /**
     * Loads |backgrounds| images if needed, and stores back the Bitmap instances
     * inside the associated backgrounds upon retrieval. Invokes |doneCallback|
     * once all backgrounds are ready to be used.
     */
    public void resolveBackgrounds(List<Background> backgrounds, Runnable doneCallback) {
        if (backgrounds == null || doneCallback == null) {
            return;
        }

        Set<ImageBackground> imageBackgrounds = new HashSet<>();
        for (Background background : backgrounds) {
            if (background instanceof ImageBackground) {
                imageBackgrounds.add((ImageBackground) background);
            }
        }

        if (imageBackgrounds.isEmpty()) {
            doneCallback.run();
            return;
        }

        // Need a final instance since the counter needs to be updated from within a
        // callback.
        final Counter counter = new Counter(imageBackgrounds.size());
        for (ImageBackground imageBackground : imageBackgrounds) {
            mImageFetcher.fetchImage(ImageFetcher.Params.create(imageBackground.imageUrl,
                                             ImageFetcher.WEB_NOTES_UMA_CLIENT_NAME),
                    bitmap -> {
                        imageBackground.setBitmap(bitmap);
                        counter.called();

                        if (counter.isDone()) {
                            doneCallback.run();
                        }
                    });
        }
    }

    /**
     * Simple counter class used to keep track of the number of images being
     * asynchronously loaded.
     */
    private class Counter {
        private int mExpectedCalls;

        public Counter(int expectedCalls) {
            mExpectedCalls = expectedCalls;
        }

        public void called() {
            --mExpectedCalls;
        }

        public boolean isDone() {
            return mExpectedCalls == 0;
        }
    }
}
