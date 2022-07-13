// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper functions for communicating between trusted and
 * untrusted. All trusted -> untrusted communication must happen through the
 * functions in this file.
 * @deprecated chrome-untrusted://personalization has been removed, but these
 * functions still exist to keep the API temporarily the same. This file should
 * be removed when possible.
 */

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import * as constants from '../common/constants.js';
import {isNonEmptyArray} from '../common/utils.js';
import {CollectionsGrid} from '../untrusted/collections_grid.js';

import {GooglePhotosEnablementState, WallpaperCollection, WallpaperImage} from './personalization_app.mojom-webui.js';

/**
 * TODO(b:197023872) this class is deprecated and should be removed by more
 * post-iframe cleanup.
 * @deprecated
 */
export class IFrameApi {
  /**
   * Send an array of wallpaper collections collections grid..
   */
  sendCollections(target: CollectionsGrid, collections: WallpaperCollection[]) {
    const event: constants.SendCollectionsEvent = {
      type: constants.EventType.SEND_COLLECTIONS,
      collections,
    };
    target.onMessageReceived(event);
  }

  /**
   * Sends whether the user is allowed to access Google Photos to collections
   * grid.
   */
  sendGooglePhotosEnabled(
      target: CollectionsGrid, enabled: GooglePhotosEnablementState) {
    const event: constants.SendGooglePhotosEnabledEvent = {
      type: constants.EventType.SEND_GOOGLE_PHOTOS_ENABLED,
      enabled,
    };
    target.onMessageReceived(event);
  }

  /**
   * Send a mapping of collectionId to the number of images in that collection.
   * A value of null for a given collection id represents that the collection
   * failed to load.
   */
  sendImageCounts(
      target: CollectionsGrid, counts: {[key: string]: number|null}) {
    const event: constants.SendImageCountsEvent = {
      type: constants.EventType.SEND_IMAGE_COUNTS,
      counts,
    };
    target.onMessageReceived(event);
  }

  /**
   * Send visibility status to a target iframe. Currently used to trigger a
   * resize event on iron-list when an iframe becomes visible again so that
   * iron-list will run layout with the current size.
   */
  sendVisible(target: CollectionsGrid, visible: boolean) {
    const event: constants.SendVisibleEvent = {
      type: constants.EventType.SEND_VISIBLE,
      visible,
    };
    target.onMessageReceived(event);
  }

  /**
   * Send an array of local images to collections grid.
   */
  sendLocalImages(
      target: CollectionsGrid,
      images: Array<FilePath|constants.DefaultImageSymbol>) {
    const event: constants.SendLocalImagesEvent = {
      type: constants.EventType.SEND_LOCAL_IMAGES,
      images,
    };
    target.onMessageReceived(event);
  }

  /**
   * Sends image data keyed by stringified image id (or default image symbol).
   */
  sendLocalImageData(
      target: CollectionsGrid,
      data: Record<string|constants.DefaultImageSymbol, string>) {
    const event: constants.SendLocalImageDataEvent = {
      type: constants.EventType.SEND_LOCAL_IMAGE_DATA,
      data,
    };
    target.onMessageReceived(event);
  }

  /**
   * Called from trusted code to validate that a received event contains valid
   * data. Ignores messages that are not of the expected type.
   */
  validateReceivedSelection(
      data: constants.Events,
      choices: WallpaperCollection[]|null): WallpaperCollection {
    assert(isNonEmptyArray(choices), 'choices must be a non-empty array');

    let selected: WallpaperCollection|WallpaperImage|undefined = undefined;
    switch (data.type) {
      case constants.EventType.SELECT_COLLECTION: {
        assert(!!data.collectionId, 'Expected a collection id parameter');
        selected = (choices as WallpaperCollection[])
                       .find(choice => choice.id === data.collectionId);
        break;
      }
      default:
        assertNotReached('Unknown event type');
    }

    assert(!!selected, 'No valid selection found in choices');
    return selected!;
  }

  static getInstance(): IFrameApi {
    return instance || (instance = new IFrameApi());
  }

  static setInstance(obj: IFrameApi) {
    instance = obj;
  }
}

let instance: IFrameApi|null = null;
