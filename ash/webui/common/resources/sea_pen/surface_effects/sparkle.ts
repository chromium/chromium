// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SPARKLE_SHADER_SOURCE from './sparkle_shader.js';
import {Vec4} from './utils.js';

const VERTEX_SHADER_SOURCE = `#version 100
precision highp float;

attribute vec2 a_position;

void main() {
  gl_Position = vec4(a_position, 0, 1);
}
`;

function createShader(
    gl: WebGLRenderingContext, type: number, source: string): WebGLShader {
  const shader = gl.createShader(type);

  if (shader == null) {
    throw new Error('Failed to create WebGLShader');
  }

  gl.shaderSource(shader, source);
  gl.compileShader(shader);

  if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
    console.warn(gl.getShaderInfoLog(shader));
    gl.deleteShader(shader);
    throw new Error('Failed to compile shader');
  }

  return shader;
}

function linkProgram(
    gl: WebGLRenderingContext, vertexShader: WebGLShader,
    fragmentShader: WebGLShader): WebGLProgram {
  const program = gl.createProgram();

  if (program == null) {
    throw new Error('Failed to create WebGLProgram');
  }

  gl.attachShader(program, vertexShader);
  gl.attachShader(program, fragmentShader);
  gl.linkProgram(program);

  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
    console.warn(gl.getProgramInfoLog(program));
    gl.deleteShader(vertexShader);
    gl.deleteShader(fragmentShader);
    gl.deleteProgram(program);
    throw new Error('Failed to link program');
  }

  return program;
}

class DrawSurface {
  private gl: WebGLRenderingContext|null = null;
  private buffer: WebGLBuffer|null = null;
  private readonly positions =
      new Float32Array([-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1]);

  constructor(gl: WebGLRenderingContext, program: WebGLProgram) {
    this.gl = gl;

    const buffer = this.buffer = gl.createBuffer();
    if (!buffer) {
      throw new Error('Failed to create WebGLBuffer for DrawSurface');
    }

    gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
    gl.bufferData(gl.ARRAY_BUFFER, this.positions, gl.STATIC_DRAW);

    const location = gl.getAttribLocation(program, 'a_position');

    gl.enableVertexAttribArray(location);
    gl.vertexAttribPointer(location, 2, gl.FLOAT, false, 0, 0);
  }

  dispose() {
    const {gl, buffer} = this;

    if (gl != null && buffer != null) {
      gl.deleteBuffer(buffer);
    }

    this.gl = null;
    this.buffer = null;
  }
}

/**
 * Sparkle implements business logic related to rendering a sparkle surface
 * effects. It abstracts canvas initialization, and manages coordinating state
 * between the browser thread and the GPU.
 */
export class Sparkle {
  private readonly canvas: HTMLCanvasElement = document.createElement('canvas');
  /**
   * An HTMLElement, ostensibly a <canvas>, within which the sparkle effect is
   * rendered.
   */
  readonly element: HTMLElement = this.canvas;

  private gl: WebGLRenderingContext|null = null;
  private instancing: ANGLE_instanced_arrays|null = null;
  private vertexShader: WebGLShader|null = null;
  private fragmentShader: WebGLShader|null = null;
  private program: WebGLProgram|null = null;

  private drawSurface: DrawSurface|null = null;

  private rendering: boolean = false;

  private width: number = -1;
  private height: number = -1;
  private readonly dpr: number = 1;  // self.devicePixelRatio;

  private topLeftBackgroundColor: Vec4 = [1.0, 1.0, 1.0, 1.0];
  private bottomRightBackgroundColor: Vec4 = [1.0, 1.0, 1.0, 1.0];
  private sparkleColor: Vec4 = [1.0, 1.0, 1.0, 1.0];
  private noiseMove: [number, number, number] = [0., 0., 0.];
  private applyNoise = false;
  private gridNum: number = 1.2;
  private lumaMatteBlendFactor: number;
  private lumaMatteOverallBrightness: number;
  private inverseLuma: number = -1.;
  private opacity: number = 1.;
  private then: number = performance.now();
  private needsUpdate: boolean = true;

  get initialized() {
    return this.gl != null;
  }

  /**
   * Sets the top left color of the background as a 4-element array. The
   * values in the array should be within [0,1].
   */
  setTopLeftBackgroundColor(color: Vec4) {
    this.topLeftBackgroundColor = color;
    this.needsUpdate = true;
  }

  /**
   * Sets the bottom right color of the background as a 4-element array. The
   * values in the array should be within [0,1].
   */
  setBottomRightBackgroundColor(color: Vec4) {
    this.bottomRightBackgroundColor = color;
    this.needsUpdate = true;
  }

  /**
   * Sets the sparkle color.
   */
  setSparkleColor(color: Vec4) {
    this.sparkleColor = color;
    this.needsUpdate = true;
  }

  /**
   * Sets the number of grid for generating noise.
   */
  setGridCount(gridNumber: number) {
    this.gridNum = gridNumber;
    this.needsUpdate = true;
  }

  /**
   * Sets blend and brightness factors of the luma matte.
   *
   * @param lumaMatteBlendFactor increases or decreases the amount of variance n
   *     noise. Setting this a lower number removes variations. I.e. the
   *     turbulence noise will look more blended. Expected input range is [0,
   *     1]. more dimmed.
   * @param lumaMatteOverallBrightness adds the overall brightness of the
   *     turbulence noise. Expected input range is [0, 1].
   *
   * Example usage: You may want to apply a small number to
   * [lumaMatteBlendFactor], such as 0.2, which makes the noise look softer.
   * However it makes the overall noise look dim, so you want offset something
   * like 0.3 for [lumaMatteOverallBrightness] to bring back its overall
   * brightness.
   */
  setLumaMatteFactors(
      lumaMatteBlendFactor: number = 1.0,
      lumaMatteOverallBrightness: number = 0.) {
    this.lumaMatteBlendFactor = lumaMatteBlendFactor;
    this.lumaMatteOverallBrightness = lumaMatteOverallBrightness;
    this.needsUpdate = true;
  }

  /**
   * Sets whether to inverse the luminosity of the noise.
   *
   * By default noise will be used as a luma matte as is. This means that you
   * will see color in the brighter area. If you want to invert it, meaning
   * blend color onto the darker side, set to true.
   */
  setInverseNoiseLuminosity(inverse: boolean) {
    this.inverseLuma = inverse ? -1. : 1.;
    this.needsUpdate = true;
  }

  /**
   * Sets the opacity to achieve fade in/ out of the animation.
   *
   * Expected value range is [1, 0].
   */
  setOpacity(opacity: number) {
    this.opacity = opacity;
    this.needsUpdate = true;
  }

  /**
   * Applies the noise offset to start noise movement.
   */
  applyNoiseOffset() {
    this.applyNoise = true;
  }

  /**
   * Resets touch state and dimensions of the sparkle.
   */
  reset() {
    this.width = -1;
    this.height = -1;
  }

  /**
   * Resizes the sparkle draw area to the specified width and height.
   */
  resize(width: number, height: number) {
    if (width === this.width && height === this.height) {
      return;
    }

    const {dpr} = this;

    this.width = width;
    this.height = height;

    // NOTE: Setting these canvas props will clear it
    this.canvas.width = width * dpr;
    this.canvas.height = height * dpr;

    const {gl} = this;

    if (gl != null) {
      gl.viewport(0, 0, width * dpr, height * dpr);
    }
  }

  /**
   * Starts the sparkle render loop.
   */
  startRendering() {
    if (this.rendering) {
      return;
    }

    this.rendering = true;
    this.render();
  }

  /**
   * Stops the sparkle render loop.
   */
  stopRendering() {
    if (this.rendering === false) {
      return;
    }

    this.rendering = false;
  }

  private render() {
    if (this.rendering === false) {
      return;
    }

    const {gl, instancing, program, dpr} = this;

    if (gl == null || instancing == null || program == null) {
      this.stopRendering();
      return;
    }

    gl.uniform1f(
        gl.getUniformLocation(program, 'u_time'),
        performance.now() - this.then);
    gl.uniform2f(
        gl.getUniformLocation(program, 'u_resolution'), this.width * dpr,
        this.height * dpr);

    const initialX = this.noiseMove[0];
    const initialY = this.noiseMove[1];
    const initialZ = this.noiseMove[2];
    if (this.applyNoise) {
      const NOISE_MOVE_OFFSET = 0.006;
      this.noiseMove = [
        initialX - NOISE_MOVE_OFFSET,
        initialY,
        initialZ + NOISE_MOVE_OFFSET,
      ];
    }
    gl.uniform3f(
        gl.getUniformLocation(program, 'u_noise_move'), ...this.noiseMove);

    if (this.needsUpdate) {
      gl.uniform4f(
          gl.getUniformLocation(program, 'u_top_left_background_color'),
          ...this.topLeftBackgroundColor);
      gl.uniform4f(
          gl.getUniformLocation(program, 'u_bottom_right_background_color'),
          ...this.bottomRightBackgroundColor);
      gl.uniform4f(
          gl.getUniformLocation(program, 'u_sparkle_color'),
          ...this.sparkleColor);
      gl.uniform1f(gl.getUniformLocation(program, 'u_grid_num'), this.gridNum);
      gl.uniform1f(
          gl.getUniformLocation(program, 'u_luma_matte_blend_factor'),
          this.lumaMatteBlendFactor);
      gl.uniform1f(
          gl.getUniformLocation(program, 'u_luma_matte_overall_brightness'),
          this.lumaMatteOverallBrightness);
      gl.uniform1f(
          gl.getUniformLocation(program, 'u_inverse_luma'), this.inverseLuma);
      gl.uniform1f(gl.getUniformLocation(program, 'u_opacity'), this.opacity);
    }
    this.needsUpdate = false;

    instancing.drawArraysInstancedANGLE(
        gl.TRIANGLES,
        0,
        6,
        10,
    );

    requestAnimationFrame(() => {
      this.render();
    });
  }

  /**
   * Initialize the sparkle. This includes initializing the WebGL context that
   * backs rendering of the sparkle.
   */
  initialize() {
    if (this.initialized) {
      return;
    }

    const gl = this.gl = this.canvas.getContext(
        'webgl', {premultipliedAlpha: true, alpha: true});

    if (gl == null) {
      return;
    }

    this.instancing = gl.getExtension('ANGLE_instanced_arrays');

    if (!this.instancing) {
      throw new Error('Could not activate WebGL instancing extension');
    }

    this.vertexShader =
        createShader(gl, gl.VERTEX_SHADER, VERTEX_SHADER_SOURCE);
    this.fragmentShader =
        createShader(gl, gl.FRAGMENT_SHADER, SPARKLE_SHADER_SOURCE);
    const program = this.program =
        linkProgram(gl, this.vertexShader, this.fragmentShader);

    gl.clearColor(0, 0, 0, 0);
    gl.enable(gl.BLEND);
    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.CULL_FACE);
    gl.blendEquation(gl.FUNC_ADD);
    gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);

    gl.useProgram(program);
    gl.uniform1f(gl.getUniformLocation(program, 'u_pixel_density'), this.dpr);

    this.drawSurface = new DrawSurface(gl, program);
  }

  /**
   * Dispose of the sparkle's internal state. After invoking this method, the
   * sparkle instance can no longer be used.
   */
  dispose() {
    if (!this.initialized) {
      return;
    }

    this.drawSurface?.dispose();

    const gl = this.gl;

    if (gl != null) {
      gl.deleteProgram(this.program);
      gl.deleteShader(this.vertexShader);
      gl.deleteShader(this.fragmentShader);
      // https://developer.mozilla.org/en-US/docs/Web/API/WEBGL_lose_context/loseContext
      gl.getExtension('WEBGL_lose_context')?.loseContext();
    }

    this.drawSurface = null;

    this.program = null;
    this.vertexShader = null;
    this.fragmentShader = null;
    this.instancing = null;
    this.gl = null;
  }
}
