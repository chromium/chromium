// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Base class for {@link Elements} to use as collection of {@link Element}s of various {@link
 * ConditionalState}s.
 */
class BaseElements {
    protected ArrayList<Element<?>> mElements = new ArrayList<>();
    protected Map<Element<?>, ElementFactory> mElementFactories = new HashMap<>();
    protected ArrayList<Condition> mOtherEnterConditions = new ArrayList<>();
    protected ArrayList<Condition> mOtherExitConditions = new ArrayList<>();

    Set<String> getElementIds() {
        Set<String> elementIds = new HashSet<>();
        for (Element<?> element : mElements) {
            elementIds.add(element.getId());
        }
        return elementIds;
    }

    List<Element<?>> getElements() {
        return mElements;
    }

    Map<Element<?>, ElementFactory> getElementFactories() {
        return mElementFactories;
    }

    List<Condition> getOtherEnterConditions() {
        return mOtherEnterConditions;
    }

    List<Condition> getOtherExitConditions() {
        return mOtherExitConditions;
    }

    void addAll(BaseElements otherElements) {
        mElements.addAll(otherElements.mElements);
        mElementFactories.putAll(otherElements.mElementFactories);
        mOtherEnterConditions.addAll(otherElements.mOtherEnterConditions);
        mOtherExitConditions.addAll(otherElements.mOtherExitConditions);
    }
}
