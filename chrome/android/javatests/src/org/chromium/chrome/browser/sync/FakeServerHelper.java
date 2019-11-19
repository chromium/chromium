// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.content.Context;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.ThreadUtils;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.components.sync.protocol.SyncEntity;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Assists in Java interaction the native Sync FakeServer.
 */
public class FakeServerHelper {
    private static final String TAG = "FakeServerHelper";

    // Lazily-instantiated singleton FakeServerHelper.
    private static FakeServerHelper sFakeServerHelper;

    // Pointer value for the FakeServer. This pointer is not owned by native
    // code, so it must be stored here for future deletion.
    private static long sNativeFakeServer;

    // The pointer to the native object called here.
    private final long mNativeFakeServerHelperAndroid;

    // Accesses the singleton FakeServerHelper. There is at most one instance created per
    // application lifetime, so no deletion mechanism is provided for the native object.
    public static FakeServerHelper get() {
        ThreadUtils.assertOnUiThread();
        if (sFakeServerHelper == null) {
            sFakeServerHelper = new FakeServerHelper();
        }
        return sFakeServerHelper;
    }

    private FakeServerHelper() {
        mNativeFakeServerHelperAndroid = nativeInit();
    }

    /**
     * Creates and configures FakeServer.
     *
     * Each call to this method should be accompanied by a later call to deleteFakeServer to avoid
     * a memory leak.
     */
    public static void useFakeServer(final Context context) {
        if (sNativeFakeServer != 0L) {
            throw new IllegalStateException(
                    "deleteFakeServer must be called before calling useFakeServer again.");
        }

        sNativeFakeServer = TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Long>() {
            @Override
            public Long call() {
                FakeServerHelper fakeServerHelper = FakeServerHelper.get();
                return fakeServerHelper.createFakeServer();
            }
        });
    }

    /**
     * Deletes the existing FakeServer.
     */
    public static void deleteFakeServer() {
        checkFakeServerInitialized("useFakeServer must be called before calling deleteFakeServer.");
        TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Void>() {
            @Override
            public Void call() {
                FakeServerHelper.get().deleteFakeServer(sNativeFakeServer);
                sNativeFakeServer = 0L;
                return null;
            }
        });
    }

    /**
     * Creates a native FakeServer object and returns its pointer. This pointer is owned by the
     * Java caller.
     *
     * @return the FakeServer pointer
     */
    public long createFakeServer() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Long>() {
            @Override
            public Long call() {
                return nativeCreateFakeServer(mNativeFakeServerHelperAndroid,
                        ProfileSyncService.get().getNativeProfileSyncServiceForTest());
            }
        });
    }

    /**
     * Deletes a native FakeServer.
     *
     * @param nativeFakeServer the pointer to be deleted
     */
    public void deleteFakeServer(final long nativeFakeServer) {
        TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Void>() {
            @Override
            public Void call() {
                nativeDeleteFakeServer(mNativeFakeServerHelperAndroid, nativeFakeServer,
                        ProfileSyncService.get().getNativeProfileSyncServiceForTest());
                return null;
            }
        });
    }

    /**
     * Returns whether {@code count} entities exist on the fake Sync server with the given
     * {@code modelType} and {@code name}.
     *
     * @param count the number of fake server entities to verify
     * @param modelType the model type of entities to verify
     * @param name the name of entities to verify
     *
     * @return whether the number of specified entities exist
     */
    public boolean verifyEntityCountByTypeAndName(
            final int count, final int modelType, final String name) {
        checkFakeServerInitialized("useFakeServer must be called before data verification.");
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return nativeVerifyEntityCountByTypeAndName(
                        mNativeFakeServerHelperAndroid, sNativeFakeServer, count, modelType, name);
            }
        });
    }

    /**
     * Verifies whether the sessions on the fake Sync server match the given set of urls.
     *
     * @param urls the set of urls to check against; order does not matter.
     *
     * @return whether the sessions on the server match the given urls.
     */
    public boolean verifySessions(final String[] urls) {
        checkFakeServerInitialized("useFakeServer must be called before data verification.");
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return nativeVerifySessions(
                        mNativeFakeServerHelperAndroid, sNativeFakeServer, urls);
            }
        });
    }

    /**
     * Returns all the SyncEntities on the fake server with the given modelType.
     *
     * @param modelType the type of entities to return.
     *
     * @return a list of all the SyncEntity protos for that type.
     */
    public List<SyncEntity> getSyncEntitiesByModelType(final int modelType)
            throws ExecutionException {
        checkFakeServerInitialized("useFakeServer must be called before getting sync entities.");
        return TestThreadUtils.runOnUiThreadBlocking(new Callable<List<SyncEntity>>() {
            @Override
            public List<SyncEntity> call() throws InvalidProtocolBufferException {
                byte[][] serializedEntities = nativeGetSyncEntitiesByModelType(
                        mNativeFakeServerHelperAndroid, sNativeFakeServer, modelType);
                List<SyncEntity> entities = new ArrayList<SyncEntity>(serializedEntities.length);
                for (int i = 0; i < serializedEntities.length; i++) {
                    entities.add(SyncEntity.parseFrom(serializedEntities[i]));
                }
                return entities;
            }
        });
    }

    /**
     * Injects an entity into the fake Sync server. This method only works for entities that will
     * eventually contain a unique client tag (e.g., preferences, typed URLs).
     *
     * @param nonUniqueName the human-readable name for the entity. This value will be used for the
     *        SyncEntity.name value
     * @param clientTag the ID that makes this entity unique across clients. This value will be used
     *        in hashed form in SyncEntity.server_defined_unique_tag
     * @param entitySpecifics the EntitySpecifics proto that represents the entity to inject
     */
    public void injectUniqueClientEntity(final String nonUniqueName, final String clientTag,
            final EntitySpecifics entitySpecifics) {
        checkFakeServerInitialized("useFakeServer must be called before data injection.");
        TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Void>() {
            @Override
            public Void call() {
                // The protocol buffer is serialized as a byte array because it can be easily
                // deserialized from this format in native code.
                nativeInjectUniqueClientEntity(mNativeFakeServerHelperAndroid, sNativeFakeServer,
                        nonUniqueName, clientTag, entitySpecifics.toByteArray());
                return null;
            }
        });
    }

    /**
     * Sets the Wallet card and address data to be served in following GetUpdates requests. Note
     * that (opposed to the native implementation) this currently only accepts a single entity,
     * because that's all we needed so far.
     *
     * @param entity the SyncEntity to serve for Wallet.
     */
    public void setWalletData(final SyncEntity entity) {
        checkFakeServerInitialized("useFakeServer must be called before data injection.");
        TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Void>() {
            @Override
            public Void call() {
                // The protocol buffer is serialized as a byte array because it can be easily
                // deserialized from this format in native code.
                nativeSetWalletData(
                        mNativeFakeServerHelperAndroid, sNativeFakeServer, entity.toByteArray());
                return null;
            }
        });
    }

    /**
     * Modify the specifics of an entity on the fake Sync server.
     *
     * @param id the ID of the entity whose specifics to modify
     * @param entitySpecifics the new specifics proto for the entity
     */
    public void modifyEntitySpecifics(final String id, final EntitySpecifics entitySpecifics) {
        checkFakeServerInitialized("useFakeServer must be called before data modification.");
        TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Void>() {
            @Override
            public Void call() {
                // The protocol buffer is serialized as a byte array because it can be easily
                // deserialized from this format in native code.
                nativeModifyEntitySpecifics(mNativeFakeServerHelperAndroid, sNativeFakeServer, id,
                        entitySpecifics.toByteArray());
                return null;
            }
        });
    }

    /**
     * Injects a bookmark into the fake Sync server.
     *
     * @param title the title of the bookmark to inject
     * @param url the URL of the bookmark to inject. This String will be passed to the native GURL
     *            class, so it must be a valid URL under its definition.
     * @param parentId the ID of the desired parent bookmark folder
     */
    public void injectBookmarkEntity(final String title, final String url, final String parentId) {
        checkFakeServerInitialized("useFakeServer must be called before data injection.");
        TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Void>() {
            @Override
            public Void call() {
                nativeInjectBookmarkEntity(
                        mNativeFakeServerHelperAndroid, sNativeFakeServer, title, url, parentId);
                return null;
            }
        });
    }

    /**
     * Injects a bookmark folder into the fake Sync server.
     *
     * @param title the title of the bookmark folder to inject
     * @param parentId the ID of the desired parent bookmark folder
     */
    public void injectBookmarkFolderEntity(final String title, final String parentId) {
        checkFakeServerInitialized("useFakeServer must be called before data injection.");
        TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Void>() {
            @Override
            public Void call() {
                nativeInjectBookmarkFolderEntity(
                        mNativeFakeServerHelperAndroid, sNativeFakeServer, title, parentId);
                return null;
            }
        });
    }

    /**
     * Modifies an existing bookmark on the fake Sync server.
     *
     * @param bookmarkId the ID of the bookmark to modify
     * @param title the new title of the bookmark
     * @param url the new URL of the bookmark. This String will be passed to the native GURL
     *            class, so it must be a valid URL under its definition.
     * @param parentId the ID of the new desired parent bookmark folder
     */
    public void modifyBookmarkEntity(
            final String bookmarkId, final String title, final String url, final String parentId) {
        checkFakeServerInitialized("useFakeServer must be called before data injection.");
        TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Void>() {
            @Override
            public Void call() {
                nativeModifyBookmarkEntity(mNativeFakeServerHelperAndroid, sNativeFakeServer,
                        bookmarkId, title, url, parentId);
                return null;
            }
        });
    }

    /**
     * Modifies an existing bookmark folder on the fake Sync server.
     *
     * @param folderId the ID of the bookmark folder to modify
     * @param title the new title of the bookmark folder
     * @param parentId the ID of the new desired parent bookmark folder
     */
    public void modifyBookmarkFolderEntity(
            final String folderId, final String title, final String parentId) {
        checkFakeServerInitialized("useFakeServer must be called before data injection.");
        TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Void>() {
            @Override
            public Void call() {
                nativeModifyBookmarkFolderEntity(mNativeFakeServerHelperAndroid, sNativeFakeServer,
                        folderId, title, parentId);
                return null;
            }
        });
    }

    /**
     * Deletes an entity on the fake Sync server.
     *
     * In other words, this method injects a tombstone into the fake Sync server.
     *
     * @param id the server ID of the entity to delete
     * @param clientTagHash the client defined unique tag hash of the entity to delete (or an empty
     *         string if sync does not care about this being a hash)
     */
    public void deleteEntity(final String id) {
        deleteEntity(id, "");
    }

    public void deleteEntity(final String id, final String clientTagHash) {
        checkFakeServerInitialized("useFakeServer must be called before deleting an entity.");
        TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Void>() {
            @Override
            public Void call() {
                nativeDeleteEntity(
                        mNativeFakeServerHelperAndroid, sNativeFakeServer, id, clientTagHash);
                return null;
            }
        });
    }

    /**
     * Returns the ID of the Bookmark Bar. This value is to be used in conjunction with
     * injectBookmarkEntity.
     *
     * @return the opaque ID of the bookmark bar entity stored in the server
     */
    public String getBookmarkBarFolderId() {
        checkFakeServerInitialized("useFakeServer must be called before access");
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<String>() {
            @Override
            public String call() {
                return nativeGetBookmarkBarFolderId(
                        mNativeFakeServerHelperAndroid, sNativeFakeServer);
            }
        });
    }

    /**
     * Clear the server data (perform dashboard stop and clear).
     */
    public void clearServerData() {
        checkFakeServerInitialized("useFakeServer must be called before clearing data");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            nativeClearServerData(mNativeFakeServerHelperAndroid, sNativeFakeServer);
        });
    }

    private static void checkFakeServerInitialized(String failureMessage) {
        if (sNativeFakeServer == 0L) {
            throw new IllegalStateException(failureMessage);
        }
    }

    // Native methods.
    private native long nativeInit();
    private native long nativeCreateFakeServer(
            long nativeFakeServerHelperAndroid, long nativeProfileSyncService);
    private native void nativeDeleteFakeServer(long nativeFakeServerHelperAndroid,
            long nativeFakeServer, long nativeProfileSyncService);
    private native boolean nativeVerifyEntityCountByTypeAndName(long nativeFakeServerHelperAndroid,
            long nativeFakeServer, int count, int modelType, String name);
    private native boolean nativeVerifySessions(
            long nativeFakeServerHelperAndroid, long nativeFakeServer, String[] urlArray);
    private native byte[][] nativeGetSyncEntitiesByModelType(
            long nativeFakeServerHelperAndroid, long nativeFakeServer, int modelType);
    private native void nativeInjectUniqueClientEntity(long nativeFakeServerHelperAndroid,
            long nativeFakeServer, String nonUniqueName, String clientTag,
            byte[] serializedEntitySpecifics);
    private native void nativeSetWalletData(
            long nativeFakeServerHelperAndroid, long nativeFakeServer, byte[] serializedEntity);
    private native void nativeModifyEntitySpecifics(long nativeFakeServerHelperAndroid,
            long nativeFakeServer, String id, byte[] serializedEntitySpecifics);
    private native void nativeInjectBookmarkEntity(long nativeFakeServerHelperAndroid,
            long nativeFakeServer, String title, String url, String parentId);
    private native void nativeInjectBookmarkFolderEntity(long nativeFakeServerHelperAndroid,
            long nativeFakeServer, String title, String parentId);
    private native void nativeModifyBookmarkEntity(long nativeFakeServerHelperAndroid,
            long nativeFakeServer, String bookmarkId, String title, String url, String parentId);
    private native void nativeModifyBookmarkFolderEntity(long nativeFakeServerHelperAndroid,
            long nativeFakeServer, String bookmarkId, String title, String parentId);
    private native String nativeGetBookmarkBarFolderId(
            long nativeFakeServerHelperAndroid, long nativeFakeServer);
    private native void nativeDeleteEntity(long nativeFakeServerHelperAndroid,
            long nativeFakeServer, String id, String clientDefinedUniqueTag);
    private native void nativeClearServerData(
            long nativeFakeServerHelperAndroid, long nativeFakeServer);
}
