// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ToteMetricFormat} from '../mojo/type.js';
import {Metadata} from '../type.js';

import {TimeLapseSaver, VideoSaver} from './video_saver.js';

/**
 * Handles captured result photos and video.
 */
export interface ResultSaver {
  /**
   * Saves photo capture result.
   *
   * @param blob Data of the photo to be added.
   * @param format Tote metric format of the photo to be added.
   * @param name Name of the photo to be saved.
   * @param metadata Data of the photo to be added.
   */
  savePhoto(
      blob: Blob, format: ToteMetricFormat, name: string,
      metadata: Metadata|null): Promise<void>;

  /**
   * Saves gif capture result.
   *
   * @param blob Data of the gif to be added.
   * @param name Name of the gif to be saved.
   */
  saveGif(blob: Blob, name: string): Promise<void>;

  /**
   * Returns a video saver to save captured result video.
   *
   * @param videoRotation Clock-wise rotation in degrees to set in the
   *     video metadata so that the saved video can be displayed in upright
   *     orientation.
   */
  startSaveVideo(videoRotation: number): Promise<VideoSaver>;

  /**
   * Saves captured video result.
   *
   * @param video Contains the video result to be saved.
   */
  finishSaveVideo(video: TimeLapseSaver|VideoSaver): Promise<void>;
}
