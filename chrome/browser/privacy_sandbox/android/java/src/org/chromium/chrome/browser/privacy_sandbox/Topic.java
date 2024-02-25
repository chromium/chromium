// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import java.util.Objects;

/**
 * Represents a CanonicalTopic consisting of a TopicId and a TaxonomyVersion (see canonical_topic.h)
 * and its display name.
 */
public final class Topic {
    private final int mTopicId;
    private final int mTaxonomyVersion;
    private final String mName;
    private final String mDescription;

    public Topic(int topicId, int taxonomyVersion, String name) {
        this(topicId, taxonomyVersion, name, "");
    }

    public Topic(int topicId, int taxonomyVersion, String name, String description) {
        mTopicId = topicId;
        mTaxonomyVersion = taxonomyVersion;
        mName = name;
        mDescription = description;
    }

    public int getTopicId() {
        return mTopicId;
    }

    public int getTaxonomyVersion() {
        return mTaxonomyVersion;
    }

    public String getName() {
        return mName;
    }

    public String getDescription() {
        return mDescription;
    }

    /**
     * Only compares topicId and taxonomyVersion since they define a topics.
     * The name is language dependent.
     */
    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof Topic)) return false;
        Topic topic = (Topic) o;
        return mTopicId == topic.mTopicId && mTaxonomyVersion == topic.mTaxonomyVersion;
    }

    /**
     * Only hashes topicId and taxonomyVersion since they define a topics.
     * The name is language dependent.
     */
    @Override
    public int hashCode() {
        return Objects.hash(mTopicId, mTaxonomyVersion);
    }

    @Override
    public String toString() {
        return "Topic{topicId="
                + mTopicId
                + ", taxonomyVersion="
                + mTaxonomyVersion
                + ", name="
                + mName
                + '}';
    }
}
