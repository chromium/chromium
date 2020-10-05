// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.identity;

/**
 * Factory for setting and retrieving instances of {@link UniqueIdentificationGenerator}s.
 * <p/>
 * A generator must always be set for a generator type before it is asked for. A generator type
 * is any string you want to use for your generator. It is typically defined as a public static
 * field in the generator itself.
 */
public final class UniqueIdentificationGeneratorFactory {

    /**
     * During startup of the application, and before any calls to
     * {@link #getInstance(String)} you must add all supported generators
     * to this factory.
     *
     * @param generatorType the type of generator this is. Must be a unique string.
     * @param gen           the generator.
     * @param force         if set to true, will override any existing generator for this type. Else
     *                      discards calls where a generator exists.
     */
    public static void registerGenerator(String generatorType, UniqueIdentificationGenerator gen,
                                         boolean force) {
        // TODO(crbug.com/1131415): remove this file after the downstream is updated.
        org.chromium.chrome.browser.uid.UniqueIdentificationGeneratorFactory.registerGenerator(
                generatorType, gen, force);
    }
}
