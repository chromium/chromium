// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options.autofill_ai;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.autofill_ai.AutofillAiOptInStatus;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityInstanceWithLabels;
import org.chromium.components.autofill.autofill_ai.EntityType;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link EntityDataManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.AUTOFILL_AI_CREATE_ENTITY_DATA_MANAGER})
public class EntityDataManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private EntityDataManager.Natives mEntityDataManagerJniMock;
    @Mock private Profile mProfile;
    @Mock private EntityInstance mEntityInstance;
    @Mock private EntityType mEntityType;
    @Mock private EntityInstanceWithLabels mEntityInstanceWithLabels;

    private EntityDataManager mEntityDataManager;
    private static final long NATIVE_PTR = 12345L;

    @Before
    public void setUp() {
        EntityDataManagerJni.setInstanceForTesting(mEntityDataManagerJniMock);

        when(mEntityDataManagerJniMock.init(any(), eq(mProfile))).thenReturn(NATIVE_PTR);
        when(mProfile.shutdownStarted()).thenReturn(false);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        mEntityDataManager = EntityDataManagerFactory.getForProfile(mProfile);
    }

    @Test
    public void testDestroy() {
        mEntityDataManager.destroy();
        verify(mEntityDataManagerJniMock).destroy(NATIVE_PTR);
    }

    @Test
    public void testRemoveEntityInstance() {
        String guid = "some-guid";
        mEntityDataManager.removeEntityInstance(guid);
        verify(mEntityDataManagerJniMock).removeEntityInstance(NATIVE_PTR, guid);
    }

    @Test
    public void testGetEntityInstance() {
        String guid = "some-guid";
        when(mEntityDataManagerJniMock.getEntityInstance(NATIVE_PTR, guid))
                .thenReturn(mEntityInstance);
        assertEquals(mEntityInstance, mEntityDataManager.getEntityInstance(guid));
    }

    @Test
    public void testAddOrUpdateEntityInstance() {
        mEntityDataManager.addOrUpdateEntityInstance(mEntityInstance);
        verify(mEntityDataManagerJniMock).addOrUpdateEntityInstance(NATIVE_PTR, mEntityInstance);
    }

    @Test
    public void testGetEntitiesWithLabels() {
        List<EntityInstanceWithLabels> entities = Arrays.asList(mEntityInstanceWithLabels);
        when(mEntityDataManagerJniMock.getEntitiesWithLabels(NATIVE_PTR)).thenReturn(entities);
        assertEquals(entities, mEntityDataManager.getEntitiesWithLabels());
    }

    @Test
    public void testGetWritableEntityTypes() {
        List<EntityType> types = Arrays.asList(mEntityType);
        when(mEntityDataManagerJniMock.getWritableEntityTypes(NATIVE_PTR)).thenReturn(types);
        assertEquals(types, mEntityDataManager.getWritableEntityTypes());
    }

    @Test
    public void testGetSortedEntityTypesForListDisplay() {
        List<EntityType> types = Arrays.asList(mEntityType);
        when(mEntityDataManagerJniMock.getSortedEntityTypesForListDisplay(NATIVE_PTR))
                .thenReturn(types);
        assertEquals(types, mEntityDataManager.getSortedEntityTypesForListDisplay());
    }

    @Test
    public void testIsEligibleToAutofillAi() {
        when(mEntityDataManagerJniMock.isEligibleToAutofillAi(NATIVE_PTR)).thenReturn(true);
        assertTrue(mEntityDataManager.isEligibleToAutofillAi());
    }

    @Test
    public void testGetAutofillAiOptInStatus() {
        when(mEntityDataManagerJniMock.getAutofillAiOptInStatus(NATIVE_PTR)).thenReturn(true);
        assertTrue(mEntityDataManager.getAutofillAiOptInStatus());
    }

    @Test
    public void testSetAutofillAiOptInStatus() {
        int status = AutofillAiOptInStatus.OPTED_IN;
        when(mEntityDataManagerJniMock.setAutofillAiOptInStatus(NATIVE_PTR, status))
                .thenReturn(true);
        assertTrue(mEntityDataManager.setAutofillAiOptInStatus(status));
        verify(mEntityDataManagerJniMock).setAutofillAiOptInStatus(NATIVE_PTR, status);
    }

    @Test
    public void testObservers() {
        EntityDataManager.EntityDataManagerObserver observer =
                mock(EntityDataManager.EntityDataManagerObserver.class);
        mEntityDataManager.registerDataObserver(observer);

        mEntityDataManager.onEntityInstancesChanged();
        verify(observer).onEntityInstancesChanged();

        mEntityDataManager.unregisterDataObserver(observer);
    }
}
