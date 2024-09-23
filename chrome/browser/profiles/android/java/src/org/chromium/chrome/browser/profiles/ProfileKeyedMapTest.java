// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import static org.chromium.chrome.browser.profiles.ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION;

import org.hamcrest.MatcherAssert;
import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.HashSet;
import java.util.Set;

/** Tests for ProfileKeyedMap. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ProfileKeyedMapTest {
    @Mock private Profile mProfile1;
    @Mock private Profile mIncognitoProfile1;
    @Mock private Profile mProfile2;
    @Mock private Profile mProfile3;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        Mockito.when(mProfile1.getOriginalProfile()).thenReturn(mProfile1);
        Mockito.when(mIncognitoProfile1.getOriginalProfile()).thenReturn(mProfile1);
    }

    @Test
    public void testReusesObjects() {
        ProfileKeyedMap<Object> map = new ProfileKeyedMap<Object>(NO_REQUIRED_CLEANUP_ACTION);

        Object obj1 = new Object();
        Assert.assertEquals(obj1, map.getForProfile(mProfile1, (profile) -> obj1));
        Assert.assertEquals(obj1, map.getForProfile(mProfile1, (profile) -> new Object()));
    }

    @Test
    public void testCleanupOnProfileDestruction() {
        Set<Object> destroyedObjects = new HashSet<>();
        ProfileKeyedMap<Object> map =
                new ProfileKeyedMap<Object>((obj) -> destroyedObjects.add(obj));

        Object obj1 = new Object();
        Assert.assertEquals(obj1, map.getForProfile(mProfile1, (profile) -> obj1));

        ProfileManager.onProfileDestroyed(mProfile1);
        MatcherAssert.assertThat(destroyedObjects, Matchers.hasItem(obj1));
    }

    @Test
    public void testDestroy() {
        Set<Object> destroyedObjects = new HashSet<>();
        ProfileKeyedMap<Object> map =
                new ProfileKeyedMap<Object>((obj) -> destroyedObjects.add(obj));

        Object obj1 = new Object();
        Assert.assertEquals(obj1, map.getForProfile(mProfile1, (profile) -> obj1));
        Object obj2 = new Object();
        Assert.assertEquals(obj2, map.getForProfile(mProfile2, (profile) -> obj2));
        Object obj3 = new Object();
        Assert.assertEquals(obj3, map.getForProfile(mProfile3, (profile) -> obj3));

        map.destroy();
        MatcherAssert.assertThat(destroyedObjects, Matchers.hasItems(obj1, obj2, obj3));
    }

    @Test
    public void testMapsAreIndependent() {
        Set<Object> destroyedObjects = new HashSet<>();
        ProfileKeyedMap<Object> map1 =
                new ProfileKeyedMap<Object>((obj) -> destroyedObjects.add(obj));

        ProfileKeyedMap<Object> map2 =
                new ProfileKeyedMap<Object>((obj) -> destroyedObjects.add(obj));

        Object obj1 = new Object();
        Assert.assertEquals(obj1, map1.getForProfile(mProfile1, (profile) -> obj1));

        Object obj2 = new Object();
        Assert.assertEquals(obj2, map2.getForProfile(mProfile1, (profile) -> obj2));

        map1.destroy();
        MatcherAssert.assertThat(destroyedObjects, Matchers.hasItem(obj1));

        Assert.assertEquals(obj2, map2.getForProfile(mProfile1, (profile) -> null));
    }

    @Test
    public void testDestroyableMap() {
        ProfileKeyedMap<Destroyable> map = ProfileKeyedMap.createMapOfDestroyables();

        Destroyable destroyable1 = Mockito.mock(Destroyable.class);
        Destroyable destroyable2 = Mockito.mock(Destroyable.class);

        map.getForProfile(mProfile1, (profile) -> destroyable1);
        map.getForProfile(mProfile2, (profile) -> destroyable2);

        Assert.assertEquals(destroyable1, map.getForProfile(mProfile1, (profile) -> null));
        Assert.assertEquals(destroyable2, map.getForProfile(mProfile2, (profile) -> null));

        map.destroy();
        Mockito.verify(destroyable1).destroy();
        Mockito.verify(destroyable2).destroy();
    }

    @Test
    public void testProfileSelection_OWN_INSTANCE() {
        ProfileKeyedMap<Object> map =
                new ProfileKeyedMap<Object>(
                        ProfileKeyedMap.ProfileSelection.OWN_INSTANCE, NO_REQUIRED_CLEANUP_ACTION);
        Object originalObj1 = new Object();
        Object incognitoObj1 = new Object();
        Assert.assertEquals(originalObj1, map.getForProfile(mProfile1, (profile) -> originalObj1));
        Assert.assertEquals(
                incognitoObj1, map.getForProfile(mIncognitoProfile1, (profile) -> incognitoObj1));
    }

    @Test
    public void testProfileSelection_REDIRECTED_TO_ORIGINAL() {
        ProfileKeyedMap<Object> map =
                new ProfileKeyedMap<Object>(
                        ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL,
                        NO_REQUIRED_CLEANUP_ACTION);
        Object originalObj1 = new Object();
        Object incognitoObj1 = new Object();
        Assert.assertEquals(originalObj1, map.getForProfile(mProfile1, (profile) -> originalObj1));
        Assert.assertEquals(
                originalObj1, map.getForProfile(mIncognitoProfile1, (profile) -> incognitoObj1));
    }
}
