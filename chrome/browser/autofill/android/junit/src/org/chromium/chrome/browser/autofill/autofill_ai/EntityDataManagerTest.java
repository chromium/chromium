// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.empty;
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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.autofill_ai.AutofillAiOptInStatus;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityInstanceWithLabels;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.utils.TestUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;

/** Unit tests for {@link EntityDataManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.AUTOFILL_AI_CREATE_ENTITY_DATA_MANAGER})
public class EntityDataManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private EntityDataManager.Natives mEntityDataManagerJniMock;
    @Mock private Profile mProfile;
    @Mock private EntityInstance mEntityInstance;

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
        Runnable localSaveFallback = () -> {};
        int descriptionStringId = 123;
        int acceptButtonStringId = 456;
        mEntityDataManager.addOrUpdateEntityInstance(
                mEntityInstance, descriptionStringId, acceptButtonStringId, localSaveFallback);
        verify(mEntityDataManagerJniMock)
                .addOrUpdateEntityInstance(
                        NATIVE_PTR,
                        mEntityInstance,
                        descriptionStringId,
                        acceptButtonStringId,
                        localSaveFallback);
    }

    @Test
    public void testGetEntitiesWithLabels() {
        List<EntityInstanceWithLabels> entities =
                Arrays.asList(
                        TestUtils.buildEntityInstanceWithLabels(
                                TestUtils.getVehicleEntityType(), "label", "sublabel"));
        when(mEntityDataManagerJniMock.getEntitiesWithLabels(NATIVE_PTR)).thenReturn(entities);
        assertEquals(entities, mEntityDataManager.getEntitiesWithLabels());
    }

    @Test
    public void testGetWritableEntityTypes() {
        List<EntityType> types = Arrays.asList(TestUtils.getVehicleEntityType());
        when(mEntityDataManagerJniMock.getWritableEntityTypes(NATIVE_PTR)).thenReturn(types);
        assertEquals(types, mEntityDataManager.getWritableEntityTypes());
    }

    @Test
    public void testGetSortedEntityTypesForListDisplay() {
        List<EntityType> types = Arrays.asList(TestUtils.getVehicleEntityType());
        when(mEntityDataManagerJniMock.getSortedEntityTypesForListDisplay(NATIVE_PTR))
                .thenReturn(types);
        assertEquals(types, mEntityDataManager.getSortedEntityTypesForListDisplay());
    }

    @Test
    public void testGetInstancesToList() {
        EntityType type1 = TestUtils.getVehicleEntityType();
        EntityType type2 = TestUtils.getPassportEntityType();
        EntityType type3 = TestUtils.getNationalIdEntityType();

        EntityInstanceWithLabels instance1 =
                TestUtils.buildEntityInstanceWithLabels(type1, "Vehicle", "");
        EntityInstanceWithLabels instance2 =
                TestUtils.buildEntityInstanceWithLabels(type2, "Passport", "");
        EntityInstanceWithLabels instance3 =
                TestUtils.buildEntityInstanceWithLabels(type1, "National ID", "");

        // Native returns types in order [type2, type1, type3]
        when(mEntityDataManagerJniMock.getSortedEntityTypesForListDisplay(NATIVE_PTR))
                .thenReturn(Arrays.asList(type2, type1, type3));
        // Native returns instances in order [instance1, instance2, instance3]
        when(mEntityDataManagerJniMock.getEntitiesWithLabels(NATIVE_PTR))
                .thenReturn(Arrays.asList(instance1, instance2, instance3));

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> result =
                mEntityDataManager.getInstancesToList();

        // Check size
        assertEquals(3, result.size());

        // Check order of keys
        assertEquals(List.of(type2, type1, type3), new ArrayList<>(result.keySet()));

        // Check values for type2
        assertEquals(1, result.get(type2).size());
        assertEquals(instance2, result.get(type2).get(0));

        // Check values for type1 (should be sorted alphabetically: A then B)
        assertEquals(List.of(instance3, instance1), result.get(type1));

        // Check values for type3 (should be empty)
        assertThat(result.get(type3), empty());
    }

    @Test
    public void testGetInstancesToList_sorting() {
        EntityType type = TestUtils.getVehicleEntityType();

        // All same type, different labels and sublabels.
        EntityInstanceWithLabels bmwX5 = TestUtils.buildEntityInstanceWithLabels(type, "BMW", "X5");
        EntityInstanceWithLabels bmw3Series =
                TestUtils.buildEntityInstanceWithLabels(type, "BMW", "3 Series");
        EntityInstanceWithLabels audiA4 =
                TestUtils.buildEntityInstanceWithLabels(type, "Audi", "A4");
        EntityInstanceWithLabels audiQ7 =
                TestUtils.buildEntityInstanceWithLabels(type, "audi", "Q7");

        when(mEntityDataManagerJniMock.getSortedEntityTypesForListDisplay(NATIVE_PTR))
                .thenReturn(Arrays.asList(type));
        when(mEntityDataManagerJniMock.getEntitiesWithLabels(NATIVE_PTR))
                .thenReturn(Arrays.asList(bmwX5, bmw3Series, audiA4, audiQ7));

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> result =
                mEntityDataManager.getInstancesToList();

        List<EntityInstanceWithLabels> sortedList = result.get(type);
        assertEquals(List.of(audiA4, audiQ7, bmw3Series, bmwX5), sortedList);
    }

    @Test
    public void testGetInstancesToList_NoInstances() {
        EntityType type1 = TestUtils.getVehicleEntityType();
        when(mEntityDataManagerJniMock.getSortedEntityTypesForListDisplay(NATIVE_PTR))
                .thenReturn(Arrays.asList(type1));
        when(mEntityDataManagerJniMock.getEntitiesWithLabels(NATIVE_PTR))
                .thenReturn(Collections.emptyList());

        LinkedHashMap<EntityType, List<EntityInstanceWithLabels>> result =
                mEntityDataManager.getInstancesToList();

        assertEquals(1, result.size());
        assertTrue(result.containsKey(type1));
        assertTrue(result.get(type1).isEmpty());
    }

    @Test
    public void testIsEligibleToAutofillAi() {
        when(mEntityDataManagerJniMock.isEligibleToAutofillAi(NATIVE_PTR)).thenReturn(true);
        assertTrue(mEntityDataManager.isEligibleToAutofillAi());
    }

    @Test
    public void testCanEnableOrDisableAutofillAi() {
        when(mEntityDataManagerJniMock.canEnableOrDisableAutofillAi(NATIVE_PTR)).thenReturn(true);
        assertTrue(mEntityDataManager.canEnableOrDisableAutofillAi());
    }

    @Test
    public void testCanListEntityInstancesInSettings() {
        when(mEntityDataManagerJniMock.canListEntityInstancesInSettings(NATIVE_PTR))
                .thenReturn(true);
        assertTrue(mEntityDataManager.canListEntityInstancesInSettings());
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
    public void testIsAutofillAiDisabledByEnterprisePolicy() {
        when(mEntityDataManagerJniMock.getIsAutofillAiDisabledByEnterprisePolicy(NATIVE_PTR))
                .thenReturn(true);
        assertTrue(mEntityDataManager.getIsAutofillAiDisabledByEnterprisePolicy());
    }

    @Test
    public void testIsAutofillAiAllowedByEnterprisePolicy() {
        when(mEntityDataManagerJniMock.getIsAutofillAiAllowedByEnterprisePolicy(NATIVE_PTR))
                .thenReturn(true);
        assertTrue(mEntityDataManager.getIsAutofillAiAllowedByEnterprisePolicy());
    }

    @Test
    public void testIsWalletPublicPassStorageEnabled() {
        when(mEntityDataManagerJniMock.isWalletPublicPassStorageEnabled(NATIVE_PTR))
                .thenReturn(true);
        assertTrue(mEntityDataManager.isWalletPublicPassStorageEnabled());
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
