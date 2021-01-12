// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.details;

import android.content.Context;
import android.view.ViewGroup;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.image_fetcher.ImageFetcher;

/**
 * Coordinator responsible for showing details.
 */
public class AssistantDetailsCoordinator {
    private final RecyclerView mView;

    public AssistantDetailsCoordinator(
            Context context, AssistantDetailsModel model, ImageFetcher imageFetcher) {
        mView = new RecyclerView(context);
        mView.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mView.setLayoutManager(new LinearLayoutManager(
                context, LinearLayoutManager.VERTICAL, /* reverseLayout= */ false));
        AssistantDetailsAdapter adapter = new AssistantDetailsAdapter(context, imageFetcher);
        mView.setAdapter(adapter);

        // Listen to the model and set the details on the adapter when they change.
        model.addObserver((source, propertyKey) -> {
            if (propertyKey == AssistantDetailsModel.DETAILS) {
                adapter.setDetails(model.get(AssistantDetailsModel.DETAILS));
            }
        });
    }

    public RecyclerView getView() {
        return mView;
    }
}
