// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from '//resources/js/assert.js';

interface ShimmerCircleProperties {
  circleColor: string;
  circleRadius: CSSUnitValue;
  circleBlur: CSSUnitValue;
  circleCenterX: CSSUnitValue;
  circleCenterY: CSSUnitValue;
  circleRadiusAmplitude: CSSUnitValue;
  circleCenterXAmplitude: CSSUnitValue;
  circleCenterYAmplitude: CSSUnitValue;
  circleRadiusWiggle: CSSUnitValue;
  circleCenterXWiggle: CSSUnitValue;
  circleCenterYWiggle: CSSUnitValue;
}

class ShimmerCircleWorklet {
  static get inputProperties() {
    return [
      '--shimmer-circle-color',
      '--shimmer-circle-radius',
      '--shimmer-circle-blur',
      '--shimmer-circle-center-x',
      '--shimmer-circle-center-y',
      '--shimmer-circle-radius-amplitude',
      '--shimmer-circle-center-x-amplitude',
      '--shimmer-circle-center-y-amplitude',
      '--shimmer-circle-radius-wiggle',
      '--shimmer-circle-center-x-wiggle',
      '--shimmer-circle-center-y-wiggle',
    ];
  }

  paint(
      ctx: PaintRenderingContext2D, size: PaintSize,
      properties: StylePropertyMapReadOnly) {
    const props = this.getAndAssertProperties(properties);

    // Get the base values from percentages to pixels.
    // We double the radius to increase the blurriness.
    const baseRadius =
        (props.circleRadius.value / 100 * Math.max(size.width, size.height)) *
        props.circleBlur.value;
    const baseCircleX = props.circleCenterX.value / 100 * size.width;
    const baseCircleY = props.circleCenterY.value / 100 * size.height;

    // Get the amplitude values from percentages to pixels.
    const radiusAmp = props.circleRadiusAmplitude.value / 100 *
        Math.max(size.width, size.height);
    const centerXAmp = props.circleCenterXAmplitude.value / 100 * size.width;
    const centerYAmp = props.circleCenterYAmplitude.value / 100 * size.height;

    // Get the actual values as they should be rendered on the screen.
    const radius = baseRadius + radiusAmp * props.circleRadiusWiggle.value;
    const centerX = baseCircleX + centerXAmp * props.circleCenterXWiggle.value;
    const centerY = baseCircleY + centerYAmp * props.circleCenterYWiggle.value;

    // Render the circle on the canvas.
    ctx.clearRect(0, 0, size.width, size.height);
    ctx.fillStyle = this.createCircleGradient(
        ctx, centerX, centerY, radius, props.circleColor);
    ctx.beginPath();
    ctx.arc(centerX, centerY, radius, 0, 2 * Math.PI);
    ctx.fill();
  }

  private createCircleGradient(
      ctx: PaintRenderingContext2D, centerX: number, centerY: number,
      radius: number, colorRgb: string) {
    // Centered radial gradient.
    const radialGradient = ctx.createRadialGradient(
        centerX,
        centerY,
        0,
        centerX,
        centerY,
        radius,
    );
    // Simulate a Gaussian full page blur by mimicking a smooth step alpha
    // change through the circle radius.
    radialGradient.addColorStop(0, this.rgbToRgba(colorRgb, 1));
    radialGradient.addColorStop(0.125, this.rgbToRgba(colorRgb, 0.957));
    radialGradient.addColorStop(0.25, this.rgbToRgba(colorRgb, 0.84375));
    radialGradient.addColorStop(0.375, this.rgbToRgba(colorRgb, 0.68359));
    radialGradient.addColorStop(0.5, this.rgbToRgba(colorRgb, 0.5));
    radialGradient.addColorStop(0.625, this.rgbToRgba(colorRgb, 0.3164));
    radialGradient.addColorStop(0.75, this.rgbToRgba(colorRgb, 0.15625));
    radialGradient.addColorStop(0.875, this.rgbToRgba(colorRgb, 0.04297));
    radialGradient.addColorStop(1, this.rgbToRgba(colorRgb, 0));
    return radialGradient;
  }

  private rgbToRgba(rgb: string, alpha: number) {
    const colorsString: string = rgb.substring(4, rgb.length - 1);
    const [red, green, blue] = colorsString.split(',');
    return `rgba(${red}, ${green}, ${blue}, ${alpha})`;
  }

  private getAndAssertProperties(properties: StylePropertyMapReadOnly):
      ShimmerCircleProperties {
    const circleColor = properties.get('--shimmer-circle-color');
    const circleRadius = properties.get('--shimmer-circle-radius');
    const circleBlur = properties.get('--shimmer-circle-blur');
    const circleCenterX = properties.get('--shimmer-circle-center-x');
    const circleCenterY = properties.get('--shimmer-circle-center-y');
    const circleRadiusAmplitude =
        properties.get('--shimmer-circle-radius-amplitude');
    const circleCenterXAmplitude =
        properties.get('--shimmer-circle-center-x-amplitude');
    const circleCenterYAmplitude =
        properties.get('--shimmer-circle-center-y-amplitude');
    const circleRadiusWiggle = properties.get('--shimmer-circle-radius-wiggle');
    const circleCenterXWiggle =
        properties.get('--shimmer-circle-center-x-wiggle');
    const circleCenterYWiggle =
        properties.get('--shimmer-circle-center-y-wiggle');

    assert(circleColor);
    assertInstanceof(circleRadius, CSSUnitValue);
    assertInstanceof(circleBlur, CSSUnitValue);
    assertInstanceof(circleCenterX, CSSUnitValue);
    assertInstanceof(circleCenterY, CSSUnitValue);
    assertInstanceof(circleRadiusAmplitude, CSSUnitValue);
    assertInstanceof(circleCenterXAmplitude, CSSUnitValue);
    assertInstanceof(circleCenterYAmplitude, CSSUnitValue);
    assertInstanceof(circleRadiusWiggle, CSSUnitValue);
    assertInstanceof(circleCenterXWiggle, CSSUnitValue);
    assertInstanceof(circleCenterYWiggle, CSSUnitValue);
    assert(
        circleRadius.unit === 'percent',
        '--shimmer-circle-radius must be a percent value');
    assert(
        circleBlur.unit === 'number',
        '--shimmer-circle-blur must be a number value');
    assert(
        circleCenterX.unit === 'percent',
        '--shimmer-circle-center-x must be a percent value');
    assert(
        circleCenterY.unit === 'percent',
        '--shimmer-circle-center-y must be a percent value');
    assert(
        circleRadiusAmplitude.unit === 'percent',
        '--shimmer-circle-radius-amplitude must be a percent value');
    assert(
        circleCenterXAmplitude.unit === 'percent',
        '--shimmer-circle-center-x-amplitude must be a percent value');
    assert(
        circleCenterYAmplitude.unit === 'percent',
        '--shimmer-circle-center-y-amplitude must be a percent value');
    assert(
        circleRadiusWiggle.unit === 'number',
        '--shimmer-circle-radius-wiggle must be a number value');
    assert(
        circleCenterXWiggle.unit === 'number',
        '--shimmer-circle-center-x-wiggle must be a number value');
    assert(
        circleCenterYWiggle.unit === 'number',
        '--shimmer-circle-center-y-wiggle must be a number value');

    return {
      circleColor: circleColor.toString(),
      circleRadius,
      circleBlur,
      circleCenterX,
      circleCenterY,
      circleRadiusAmplitude,
      circleCenterXAmplitude,
      circleCenterYAmplitude,
      circleRadiusWiggle,
      circleCenterXWiggle,
      circleCenterYWiggle,
    };
  }
}

registerPaint('shimmer-circle', ShimmerCircleWorklet);
