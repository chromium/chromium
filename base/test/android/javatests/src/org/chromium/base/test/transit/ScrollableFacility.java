// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;

import static org.hamcrest.CoreMatchers.is;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.IntDef;
import androidx.test.espresso.PerformException;
import androidx.test.espresso.Root;
import androidx.test.espresso.action.ViewActions;

import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.ScrollableFacility.Item.Presence;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Represents a facility that contains items which may or may not be visible due to scrolling.
 *
 * @param <HostStationT> the type of host {@link Station} this is scoped to.
 */
@NullMarked
public abstract class ScrollableFacility<HostStationT extends Station<?>>
        extends Facility<HostStationT> {

    private @MonotonicNonNull ArrayList<Item> mItems;

    /** Must populate |items| with the expected items. */
    protected abstract void declareItems(ItemsBuilder items);

    /**
     * Returns the minimum number of items declared expected to be displayed screen initially.
     *
     * <p>Defaults to 2.
     */
    protected int getMinimumOnScreenItemCount() {
        return 2;
    }

    @CallSuper
    @Override
    public void declareExtraElements() {
        mItems = new ArrayList<>();
        declareItems(new ItemsBuilder(mItems));

        int i = 0;
        int itemsToExpect = getMinimumOnScreenItemCount();
        for (Item item : mItems) {
            // Expect only the first |itemsToExpect| items because of scrolling.
            // Items that should be absent should be checked regardless of position.
            if (item.getPresence() == Presence.ABSENT || i < itemsToExpect) {
                switch (item.mPresence) {
                    case Presence.ABSENT:
                        assert item.mViewSpec != null;
                        declareNoView(item.mViewSpec.getViewMatcher());
                        break;
                    case Presence.PRESENT_AND_ENABLED:
                    case Presence.PRESENT_AND_DISABLED:
                        assert item.mViewSpec != null;
                        assert item.mViewElementOptions != null;
                        declareView(item.mViewSpec, item.mViewElementOptions);
                        break;
                    case Presence.MAYBE_PRESENT:
                    case Presence.MAYBE_PRESENT_STUB:
                        // No ViewElements are declared.
                        break;
                }
            }

            i++;
        }
    }

    /**
     * Subclasses' {@link #declareItems(ItemsBuilder)} should declare items through ItemsBuilder.
     */
    public class ItemsBuilder {
        private final List<Item> mItems;

        private ItemsBuilder(List<Item> items) {
            mItems = items;
        }

        /** Create a new item. */
        public Item declareItem(
                ViewSpec<? extends View> onScreenViewSpec,
                @Nullable Matcher<?> offScreenDataMatcher) {
            Item item =
                    new Item(onScreenViewSpec, offScreenDataMatcher, Presence.PRESENT_AND_ENABLED);
            mItems.add(item);
            return item;
        }

        /** Create a new disabled item. */
        public Item declareDisabledItem(
                ViewSpec<? extends View> onScreenViewSpec,
                @Nullable Matcher<?> offScreenDataMatcher) {
            Item item =
                    new Item(onScreenViewSpec, offScreenDataMatcher, Presence.PRESENT_AND_DISABLED);
            mItems.add(item);
            return item;
        }

        /** Create a new item expected to be absent. */
        public Item declareAbsentItem(
                ViewSpec<? extends View> onScreenViewSpec,
                @Nullable Matcher<?> offScreenDataMatcher) {
            Item item = new Item(onScreenViewSpec, offScreenDataMatcher, Presence.ABSENT);
            mItems.add(item);
            return item;
        }

        /** Create a new item which may or may not be present. */
        public Item declarePossibleItem(
                ViewSpec<? extends View> onScreenViewSpec,
                @Nullable Matcher<?> offScreenDataMatcher) {
            Item item = new Item(onScreenViewSpec, offScreenDataMatcher, Presence.MAYBE_PRESENT);
            mItems.add(item);
            return item;
        }

        /** Create a new item stub which may or may not be present. */
        public Item declarePossibleStubItem() {
            Item item =
                    new Item(
                            /* onScreenViewSpec= */ null,
                            /* offScreenDataMatcher= */ null,
                            Presence.MAYBE_PRESENT_STUB);
            mItems.add(item);
            return item;
        }
    }

    /**
     * Represents an item in a specific {@link ScrollableFacility}.
     *
     * <p>{@link ScrollableFacility} subclasses should use these to represent their items.
     */
    public class Item {

        /** Whether the item is expected to be present and enabled. */
        @IntDef({
            Presence.ABSENT,
            Presence.PRESENT_AND_ENABLED,
            Presence.PRESENT_AND_DISABLED,
            Presence.MAYBE_PRESENT,
            Presence.MAYBE_PRESENT_STUB,
        })
        @Retention(RetentionPolicy.SOURCE)
        public @interface Presence {
            // Item must not be present.
            int ABSENT = 0;

            // Item must be present and enabled.
            int PRESENT_AND_ENABLED = 1;

            // Item must be present and disabled.
            int PRESENT_AND_DISABLED = 2;

            // No expectations on item being present or enabled.
            int MAYBE_PRESENT = 3;

            // No expectations on item being present or enabled, and select trigger is not
            // implemented. Some optimizations can be made.
            int MAYBE_PRESENT_STUB = 4;
        }

        protected final @Nullable Matcher<?> mOffScreenDataMatcher;
        protected final @Presence int mPresence;
        protected final @Nullable ViewSpec<? extends View> mViewSpec;
        protected final ViewElement.@Nullable Options mViewElementOptions;

        /**
         * Use one of {@link ScrollableFacility.ItemsBuilder}'s methods to instantiate:
         *
         * <ul>
         *   <li>{@link ItemsBuilder#declareItem(ViewSpec, Matcher)}
         *   <li>{@link ItemsBuilder#declareDisabledItem(ViewSpec, Matcher)}
         *   <li>{@link ItemsBuilder#declareAbsentItem(ViewSpec, Matcher)}
         *   <li>{@link ItemsBuilder#declarePossibleItem(ViewSpec, Matcher)}
         *   <li>{@link ItemsBuilder#declarePossibleStubItem()}
         * </ul>
         */
        protected Item(
                @Nullable ViewSpec<? extends View> onScreenViewSpec,
                @Nullable Matcher<?> offScreenDataMatcher,
                @Presence int presence) {
            mPresence = presence;
            mOffScreenDataMatcher = offScreenDataMatcher;

            switch (mPresence) {
                case Presence.ABSENT:
                    assert onScreenViewSpec != null;
                    mViewSpec = onScreenViewSpec;
                    mViewElementOptions = null;
                    break;
                case Presence.MAYBE_PRESENT_STUB:
                    assert onScreenViewSpec == null;
                    mViewSpec = null;
                    mViewElementOptions = null;
                    break;
                case Presence.PRESENT_AND_ENABLED:
                case Presence.MAYBE_PRESENT:
                    assert onScreenViewSpec != null;
                    mViewSpec = onScreenViewSpec;
                    mViewElementOptions = ViewElement.Options.DEFAULT;
                    break;
                case Presence.PRESENT_AND_DISABLED:
                    assert onScreenViewSpec != null;
                    mViewSpec = onScreenViewSpec;
                    mViewElementOptions = ViewElement.expectDisabledOption();
                    break;
                default:
                    mViewSpec = null;
                    mViewElementOptions = null;
                    assert false;
            }
        }

        /** Select the item, scrolling to it if necessary, to start a Transition. */
        public TripBuilder scrollToAndSelectWithoutClosingTo() {
            return scrollToItemIfNeeded().selectTo();
        }

        /**
         * Select the item, scrolling to it if necessary, to start a Transition which will exit the
         * scrollable.
         */
        public TripBuilder scrollToAndSelectTo() {
            return scrollToItemIfNeeded()
                    .selectTo()
                    .exitFacilityAnd()
                    .exitFacilityAnd(ScrollableFacility.this);
        }

        /**
         * Scroll to the item if necessary.
         *
         * @return a ItemScrolledTo facility representing the item on the screen, which can be
         *     further interacted with.
         */
        public ItemOnScreenFacility scrollToItemIfNeeded() {
            assert mPresence != Presence.ABSENT;

            // Could in theory try to scroll to a stub, but not supporting this prevents the
            // creation of a number of objects that are likely not going to be used.
            assert mPresence != Presence.MAYBE_PRESENT_STUB;

            ItemOnScreenFacility focusedItem = new ItemOnScreenFacility(this);

            assumeNonNull(mViewSpec);

            Root root = determineRoot();
            assert root != null;

            List<ViewAndRoot> viewMatches =
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> ViewFinder.findViews(List.of(root), mViewSpec.getViewMatcher()));
            if (viewMatches.size() > 1) {
                throw new IllegalStateException(
                        ViewConditions.writeMatchingViewsStatusMessage(viewMatches));
            }

            if (viewMatches.size() == 1) {
                if (isCompletelyDisplayed().matches(viewMatches.get(0).view)) {
                    return noopTo().enterFacility(focusedItem);
                }
            }

            return scrollToItemTo().enterFacility(focusedItem);
        }

        public @Presence int getPresence() {
            return mPresence;
        }

        public ViewSpec<View> getViewSpec() {
            assert mViewSpec != null : "Trying to get a ViewSpec for an item not present.";
            return (ViewSpec<View>) mViewSpec;
        }

        public ViewElement.Options getViewElementOptions() {
            assert mViewElementOptions != null
                    : "Trying to get ViewElement.Options for an item not present.";
            return mViewElementOptions;
        }

        /** Scroll to the item to start a Transition. */
        public TripBuilder scrollToItemTo() {
            Root root = determineRoot();
            assert root != null;

            if (mOffScreenDataMatcher != null) {
                // If there is a data matcher, use it to scroll as the item might be in a
                // RecyclerView.
                try {
                    return runTo(
                            () ->
                                    onData(mOffScreenDataMatcher)
                                            .inRoot(withDecorView(is(root.getDecorView())))
                                            .perform(ViewActions.scrollTo()));
                } catch (PerformException performException) {
                    throw TravelException.newTravelException(
                            String.format(
                                    "Could not scroll using data matcher %s",
                                    mOffScreenDataMatcher),
                            performException);
                }
            } else {
                // If there is no data matcher, use the ViewMatcher to scroll as the item should be
                // created but not displayed.
                assumeNonNull(mViewSpec);
                try {
                    return runTo(
                            () ->
                                    onView(mViewSpec.getViewMatcher())
                                            .inRoot(withDecorView(is(root.getDecorView())))
                                            .perform(ViewActions.scrollTo()));
                } catch (PerformException performException) {
                    throw TravelException.newTravelException(
                            String.format(
                                    "Could not scroll using view matcher %s",
                                    mViewSpec.getViewMatcher()),
                            performException);
                }
            }
        }
    }

    private @Nullable Root determineRoot() {
        assertInPhase(Phase.ACTIVE);

        // Get the root of the first ViewElement declared.
        for (Element<?> e : getElements().getElements()) {
            if (e instanceof ViewElement<?> viewElement) {
                return viewElement.getDisplayedCondition().getRootMatched();
            }
        }
        throw new IllegalStateException("No ViewElement found in " + this);
    }

    /** Get all {@link Item}s declared in this {@link ScrollableFacility}. */
    public List<Item> getItems() {
        assert mItems != null : "declareItems() not called yet.";
        return mItems;
    }

    /** A facility representing an item inside a {@link ScrollableFacility} shown on the screen. */
    public class ItemOnScreenFacility extends Facility<HostStationT> {

        protected final Item mItem;
        public @MonotonicNonNull ViewElement<View> viewElement;

        protected ItemOnScreenFacility(Item item) {
            mItem = item;
        }

        @Override
        public void declareExtraElements() {
            viewElement = declareView(mItem.getViewSpec(), mItem.getViewElementOptions());
        }

        /** Select the item to start a Transition. */
        public TripBuilder selectTo() {
            assert viewElement != null;
            return viewElement.clickTo();
        }

        /** Returns the {@link Item} that is on the screen. */
        public Item getItem() {
            return mItem;
        }
    }

    /** Scroll to each declared item and check they are there with the expected enabled state. */
    public void verifyPresentItems() {
        for (ScrollableFacility<?>.Item item : getItems()) {
            if (item.getPresence() == Presence.PRESENT_AND_ENABLED
                    || item.getPresence() == Presence.PRESENT_AND_DISABLED) {
                try {
                    item.scrollToItemIfNeeded();
                } catch (Exception e) {
                    // Wrap exception to make clear which item was being scroll to
                    StringBuilder sb = new StringBuilder();
                    sb.append("Item [");
                    sb.append(StringDescription.asString(item.getViewSpec().getViewMatcher()));
                    if (item.mOffScreenDataMatcher != null) {
                        sb.append(" / ");
                        sb.append(StringDescription.asString(item.mOffScreenDataMatcher));
                    }
                    sb.append("] not present");
                    throw new AssertionError(sb.toString(), e);
                }
            }
        }
    }
}
