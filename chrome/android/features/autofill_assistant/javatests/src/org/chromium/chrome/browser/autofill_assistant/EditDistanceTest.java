// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.verify;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.InOrder;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.ui.modelutil.ListModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Unit test suite for {@link EditDistance}.
 */
@RunWith(JUnit4.class)
public class EditDistanceTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    @SmallTest
    public void testEmptySource() {
        ListModel<Integer> spiedListModel = createSpiedListModel();
        testTransformation(spiedListModel, Arrays.asList(1, 2, 3));
        InOrder inOrder = inOrder(spiedListModel);
        inOrder.verify(spiedListModel).add(0, 3);
        inOrder.verify(spiedListModel).add(0, 2);
        inOrder.verify(spiedListModel).add(0, 1);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/963672")
    public void testEmptyTarget() {
        ListModel<Integer> spiedListModel = createSpiedListModel(1, 2, 3);
        testTransformation(spiedListModel, Collections.emptyList());
        InOrder inOrder = inOrder(spiedListModel);
        inOrder.verify(spiedListModel).removeAt(2);
        inOrder.verify(spiedListModel).removeAt(1);
        inOrder.verify(spiedListModel).removeAt(0);
    }

    @Test
    @SmallTest
    public void testEmptySourceAndEmptyTarget() {
        testTransformation(createSpiedListModel(), Collections.emptyList());
    }

    @Test
    @SmallTest
    public void testEqualSourceAndTarget() {
        testTransformation(createSpiedListModel(1, 2, 3), Arrays.asList(1, 2, 3));
    }

    @Test
    @SmallTest
    public void testInsertBeginning() {
        ListModel<Integer> spiedListModel = createSpiedListModel(2, 3);
        testTransformation(spiedListModel, Arrays.asList(1, 2, 3));
        verify(spiedListModel).add(0, 1);
    }

    @Test
    @SmallTest
    public void testInsertMiddle() {
        ListModel<Integer> spiedListModel = createSpiedListModel(1, 3);
        testTransformation(spiedListModel, Arrays.asList(1, 2, 3));
        verify(spiedListModel).add(1, 2);
    }

    @Test
    @SmallTest
    public void testInsertEnd() {
        ListModel<Integer> spiedListModel = createSpiedListModel(1, 2);
        testTransformation(spiedListModel, Arrays.asList(1, 2, 3));
        verify(spiedListModel).add(2, 3);
    }

    @Test
    @SmallTest
    public void testDeleteBeginning() {
        ListModel<Integer> spiedListModel = createSpiedListModel(1, 2, 3);
        testTransformation(spiedListModel, Arrays.asList(2, 3));
        verify(spiedListModel).removeAt(0);
    }

    @Test
    @SmallTest
    public void testDeleteMiddle() {
        ListModel<Integer> spiedListModel = createSpiedListModel(1, 2, 3);
        testTransformation(spiedListModel, Arrays.asList(1, 3));
        verify(spiedListModel).removeAt(1);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/963672")
    public void testDeleteEnd() {
        ListModel<Integer> spiedListModel = createSpiedListModel(1, 2, 3);
        testTransformation(spiedListModel, Arrays.asList(1, 2));
        verify(spiedListModel).removeAt(2);
    }

    @Test
    @SmallTest
    public void testSubstitutionBeginning() {
        ListModel<Integer> spiedListModel = createSpiedListModel(0, 2, 3);
        testTransformation(spiedListModel, Arrays.asList(1, 2, 3));
        verify(spiedListModel).update(0, 1);
    }

    @Test
    @SmallTest
    public void testSubstitutionMiddle() {
        ListModel<Integer> spiedListModel = createSpiedListModel(1, 0, 3);
        testTransformation(spiedListModel, Arrays.asList(1, 2, 3));
        verify(spiedListModel).update(1, 2);
    }

    @Test
    @SmallTest
    public void testSubstitutionEnd() {
        ListModel<Integer> spiedListModel = createSpiedListModel(1, 2, 0);
        testTransformation(spiedListModel, Arrays.asList(1, 2, 3));
        verify(spiedListModel).update(2, 3);
    }

    @Test
    @SmallTest
    public void testMultipleInsert() {
        ListModel<Integer> spiedListModel = createSpiedListModel(3, 6);
        testTransformation(spiedListModel, Arrays.asList(1, 2, 3, 4, 5, 6, 7, 8));
        InOrder inOrder = inOrder(spiedListModel);
        inOrder.verify(spiedListModel).add(2, 8);
        inOrder.verify(spiedListModel).add(2, 7);
        inOrder.verify(spiedListModel).add(1, 5);
        inOrder.verify(spiedListModel).add(1, 4);
        inOrder.verify(spiedListModel).add(0, 2);
        inOrder.verify(spiedListModel).add(0, 1);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/963672")
    public void testMultipleDelete() {
        ListModel<Integer> spiedListModel = createSpiedListModel(1, 2, 3, 4, 5, 6, 7, 8);
        testTransformation(spiedListModel, Arrays.asList(3, 6));
        InOrder inOrder = inOrder(spiedListModel);
        inOrder.verify(spiedListModel).removeAt(7);
        inOrder.verify(spiedListModel).removeAt(6);
        inOrder.verify(spiedListModel).removeAt(4);
        inOrder.verify(spiedListModel).removeAt(3);
        inOrder.verify(spiedListModel).removeAt(1);
        inOrder.verify(spiedListModel).removeAt(0);
    }

    @Test
    @SmallTest
    public void testMultipleSubstitutions() {
        ListModel<Integer> spiedListModel = createSpiedListModel(0, 0, 3, 0, 0, 6, 0, 0);
        testTransformation(spiedListModel, Arrays.asList(1, 2, 3, 4, 5, 6, 7, 8));
        verify(spiedListModel).update(0, 1);
        verify(spiedListModel).update(1, 2);
        verify(spiedListModel).update(3, 4);
        verify(spiedListModel).update(4, 5);
        verify(spiedListModel).update(6, 7);
        verify(spiedListModel).update(7, 8);
    }

    @Test
    @SmallTest
    public void testPopAndAppend() {
        ListModel<Integer> spiedListModel = createSpiedListModel(1, 2, 3);
        testTransformation(spiedListModel, Arrays.asList(2, 3, 4));
        InOrder inOrder = inOrder(spiedListModel);
        inOrder.verify(spiedListModel).add(3, 4);
        inOrder.verify(spiedListModel).removeAt(0);
    }

    @Test
    @SmallTest
    public void testAppendAndPop() {
        ListModel<Integer> spiedListModel = createSpiedListModel(1, 2, 3);
        testTransformation(spiedListModel, Arrays.asList(0, 1, 2));
        InOrder inOrder = inOrder(spiedListModel);
        inOrder.verify(spiedListModel).removeAt(2);
        inOrder.verify(spiedListModel).add(0, 0);
    }

    @Test
    @SmallTest
    public void testMixedOperations() {
        ListModel<Integer> spiedListModel = createSpiedListModel(1, 2, 3, 4, 5, 6);
        testTransformation(spiedListModel, Arrays.asList(2, 8, 4, 5, 7, 6));

        // 0-cost substitutions.
        verify(spiedListModel).update(1, 2);
        verify(spiedListModel).update(3, 4);
        verify(spiedListModel).update(4, 5);
        verify(spiedListModel).update(5, 6);

        verify(spiedListModel).update(2, 8);
        InOrder inOrder = inOrder(spiedListModel);
        inOrder.verify(spiedListModel).add(5, 7);
        inOrder.verify(spiedListModel).removeAt(0);
    }

    private void testTransformation(ListModel<Integer> source, List<Integer> target) {
        EditDistance.transform(source, target, Integer::equals);

        List<Integer> sourceList = new ArrayList<>();
        for (Integer value : source) {
            sourceList.add(value);
        }
        Assert.assertEquals(target, sourceList);
    }

    private static ListModel<Integer> createSpiedListModel(Integer... values) {
        ListModel<Integer> model = new ListModel<>();
        model.set(values);
        return Mockito.spy(model);
    }
}
