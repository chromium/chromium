# Tab Strip
This component refers to the list of tabs shown on top of the toolbar on larger screens.

## Rendering Tab Strip
[StripLayoutHelperManager](StripLayoutHelperManager.java) is registered as a SceneOverlay for tablets. During compositor layout, this scene overlay's updateOverlay(...) -> StripLayoutHelper#updateLayout is invoked to compute and set required properties for all child layers (tabs, new tab button, etc.).

During compositorView layer finalization, getUpdatedSceneOverlayTree(...) is invoked which pushes all the child view properties along with strip properties (scrim, fades etc) to CC layer via TabStripSceneLayer JNI. This updates the existing layers, creates any missing layers, and culls any layers that are no longer visible in the tab strip's layer tree. These layers eventually get composited (along with the other scene overlays).

For more info on compositor, refer to [cc/README](https://source.chromium.org/chromium/chromium/src/+/main:cc/README.md)

Layout passes (i.e. #updateOverlay calls) are triggered automatically through CompositorAnimators or manually through LayoutUpdateHost#requestUpdate. #requestUpdate informs the layout manager that a layout pass is needed and requests a render. This is needed for any event that can change the size or position of any of the composited layers. A new frame can be drawn without a layout pass by directly requesting a render (i.e. calling LayoutRenderHost#requestRender). This hints to the compositor that a new frame is needed, which will pull all of the properties from the TabStripSceneLayer. This can be done for events that don't affect the size or position of the StripLayoutViews (e.g. changing tint or title bitmap).

## Code Structure
__StripLayoutHelperManager__ is the coordinator for this component. This class manages 2 instances of StripLayoutHelper for standard and incognito strips. Specifically, it routes external events (motion events, layout change, tab model updates, etc.) to the active StripLayoutHelper instance.

__StripLayoutHelper__ Mediator for tab strip views and view updates.
* __StripLayoutView__ is the interface for child views on the strip. This is implemented by tab, CompositorButton (buttons on strip)  and group titles. Essentially just a POD type that holds position/size data for a conceptual layer that takes up space on the tab strip. Note that some layers don't take up space and have static positions/sizes, so they don't use this interface (e.g. fades are anchored to the ends of the strip or dividers are children of tab layers).
* __ScrollDelegate__ is a delegate that manages scroll offsets. This uses  __StackScroller__ which is a copy of Android's OverScroller and built to pass app time for scroll computations. __StripStacker__ computes offsets for each strip view.
* __ReorderDelegate__ is a delegate that manages reorder logic for tabs.
* __TabDragSource__ is a drag event listener for any drags and drops occurring on the tab strip
* __TabLoadTracker__ tracks whether a tab is loading or not and runs actions accordingly. Currently only used to mark whether a tab should show the loading spinner or its favicon.