// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

/**
 * Interface to deserialize an extension from the data contained in a {@link TaskTraits} instance by
 * passing it to the {@link TaskTraits#getExtension} method.
 *
 * @param <Extension>
 * @see TaskTraits#getExtension
 */
public interface TaskTraitsExtensionDescriptor<Extension> {
    /**
     * Used by {@link TaskTraits} to make sure that extension data can be deserialized with this
     * descriptor.
     *
     * @return The id for the extension that can be deserialized by this class.
     */
    int getId();

    /**
     * Deserializes an extension for the data contained in {@link TaskTraits}. Caller must make sure
     * that the data is for this extension by matching the extension id contained in the
     * {@link TaskTraits} by comparing it to the one returned by {@link #getId}
     *
     * @param data serialized extension data
     * @return Extension instance
     */
    Extension fromSerializedData(byte[] data);

    /**
     * Serializes an extension as data contained in {@link TaskTraits}.
     *
     * @param extension Extension instance
     * @return serialized extension data
     */
    byte[] toSerializedData(Extension extension);
}
