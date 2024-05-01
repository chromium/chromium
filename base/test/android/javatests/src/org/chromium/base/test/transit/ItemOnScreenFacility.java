// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

/**
 * A facility representing an item inside a {@link ScrollableFacility} shown on the screen.
 *
 * @param <HostStationT> the type of TransitStation this is scoped to.
 * @param <SelectReturnT> the return type of the |selectHandler|.
 */
public class ItemOnScreenFacility<HostStationT extends TransitStation, SelectReturnT>
        extends StationFacility<HostStationT> {

    protected final ScrollableFacility<HostStationT>.Item<SelectReturnT> mItem;

    protected ItemOnScreenFacility(
            HostStationT station, ScrollableFacility<HostStationT>.Item<SelectReturnT> item) {
        super(station);
        mItem = item;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        mItem.declareViewElement(elements);
    }

    /** Select the item and trigger its |selectHandler|. */
    public SelectReturnT select() {
        if (!mItem.isPresent()) {
            throw new IllegalStateException("Cannot click on an absent item");
        }
        if (!mItem.isEnabled()) {
            throw new IllegalStateException("Cannot click on a disabled item");
        }

        try {
            return mItem.getSelectHandler().call();
        } catch (Exception e) {
            throw TravelException.newTravelException("Select handler threw an exception:", e);
        }
    }
}
