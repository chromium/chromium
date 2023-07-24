// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.readingMode API */

declare namespace chrome {
  export namespace readingMode {
    /////////////////////////////////////////////////////////////////////
    // Implemented in read_anything_app_controller.cc and consumed by ts.
    /////////////////////////////////////////////////////////////////////

    // The root AXNodeID of the tree to be displayed.
    let rootId: number;

    // Selection information. The selection must be a forward selection, with
    // the start node and offset before the end node and offset. Through
    // experimentation, it was observed that programmatically created backwards
    // ranges are collapsed to the start node. A test was added in
    // read_anything_app_test to confirm this assumption.
    let startNodeId: number;
    let startOffset: number;
    let endNodeId: number;
    let endOffset: number;

    // Items in the ReadAnythingTheme struct, see read_anything.mojom for info.
    let fontName: string;
    let fontSize: number;
    let foregroundColor: number;
    let backgroundColor: number;
    let lineSpacing: number;
    let letterSpacing: number;

    // Whether the WebUI toolbar feature flag is enabled.
    let isWebUIToolbarVisible: boolean;

    // Indicates if select-to-distill works on the web page. Used to
    // determine which empty state to display.
    let isSelectable: boolean;

    // Returns a list of AXNodeIDs corresponding to the unignored children of
    // the AXNode for the provided AXNodeID. If there is a selection contained
    // in this node, only returns children which are partially or entirely
    // contained within the selection.
    function getChildren(nodeId: number): number[];

    // Returns the HTML tag of the AXNode for the provided AXNodeID.
    function getHtmlTag(nodeId: number): string;

    // Returns the language of the AXNode for the provided AXNodeID.
    function getLanguage(nodeId: number): string;

    // Returns the text content of the AXNode for the provided AXNodeID. If a
    // selection begins or ends in this node, truncates the text to only return
    // the selected text.
    function getTextContent(nodeId: number): string;

    // Returns the text direction of the AXNode for the provided AXNodeID.
    function getTextDirection(nodeId: number): string;

    // Returns the url of the AXNode for the provided AXNodeID.
    function getUrl(nodeId: number): string;

    // Returns true if the text node / element should be bolded.
    function shouldBold(nodeId: number): boolean;

    // Returns true if the element has overline text styling.
    function isOverline(nodeId: number): boolean;

    // Connects to the browser process. Called by ts when the read anything
    // element is added to the document.
    function onConnected(): void;

    // Called when a user tries to copy text from reading mode with keyboard
    // shortcuts.
    function onCopy(): void;

    // Called when the Read Anything panel is scrolled.
    function onScroll(onSelection: boolean): void;

    // Called when a user clicks a link. NodeID is an AXNodeID which identifies
    // the link's corresponding AXNode in the main pane.
    function onLinkClicked(nodeId: number): void;

    // Called when a user makes a selection change. AnchorNodeID and
    // focusAXNodeID are AXNodeIDs which identify the anchor and focus AXNodes
    // in the main pane. The selection can either be forward or backwards.
    function onSelectionChange(
        anchorNodeId: number, anchorOffset: number, focusNodeId: number,
        focusOffset: number): void;

    // Set the content. Used by tests only.
    // SnapshotLite is a data structure which resembles an AXTreeUpdate. E.g.:
    //   const axTree = {
    //     rootId: 1,
    //     nodes: [
    //       {
    //         id: 1,
    //         role: 'rootWebArea',
    //         childIds: [2],
    //       },
    //       {
    //         id: 2,
    //         role: 'staticText',
    //         name: 'Some text.',
    //       },
    //     ],
    //   };
    function setContentForTesting(
        snapshotLite: Object, contentNodeIds: number[]): void;

    // Set the theme. Used by tests only.
    function setThemeForTesting(
        fontName: string, fontSize: number, foregroundColor: number,
        backgroundColor: number, lineSpacing: number,
        letterSpacing: number): void;

    ////////////////////////////////////////////////////////////////
    // Implemented in read_anything/app.ts and called by native c++.
    ////////////////////////////////////////////////////////////////

    // Display a loading screen to tell the user we are distilling the page.
    function showLoading(): void;

    // Display the empty state page to tell the user we can't distill the page.
    function showEmpty(): void;

    // Ping that an AXTree has been distilled for the active tab's render frame
    // and is available to consume.
    function updateContent(): void;

    // Ping that the selection has been updated.
    function updateSelection(): void;

    // Ping that the theme choices of the user have been changed using the
    // toolbar and are ready to consume.
    function updateTheme(): void;
  }
}
