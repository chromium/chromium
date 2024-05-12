// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_shared.css.js';
import './strings.m.js';
import 'chrome://resources/d3/d3.min.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './realtime_cpu_chart.html.js';

export interface ChartPadding {
  top: number;
  right: number;
  bottom: number;
  left: number;
  tick: number;
}

const RealtimeCpuChartElementBase = I18nMixin(PolymerElement);

/**
 * @fileoverview
 * 'realtime-cpu-chart' is a moving stacked area graph component used to display
 * a realtime cpu usage information.
 */

export class RealtimeCpuChartElement extends RealtimeCpuChartElementBase {
  static get is(): 'realtime-cpu-chart' {
    return 'realtime-cpu-chart' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      user: {
        type: Number,
        value: 0,
      },

      system: {
        type: Number,
        value: 0,
      },

      numDataPoints: {
        type: Number,
        value: 50,
      },

      dataRefreshPerSecond: {
        type: Number,
        value: 2,
      },

      /**
       * Chart rendering frames per second.
       * Strongly preferred to be multiples of dataRefreshPerSecond. If not,
       * there will be a small (hard to notice) jittering at every data refresh.
       */
      framesPerSecond: {
        type: Number,
        value: 30,
      },

      /**
       * Duration of each frame in milliseconds.
       */
      frameDuration: {
        readOnly: true,
        type: Number,
        computed: 'getFrameDuration(dataRefreshPerSecond, framesPerSecond)',
      },

      width: {
        type: Number,
        value: 560,
      },

      height: {
        type: Number,
        value: 114,
      },

      padding: {
        type: Object,
        value: {top: 10, right: 5, bottom: 8, left: 50, tick: 10},
      },

      graphWidth: {
        readOnly: true,
        type: Number,
        computed: 'getGraphDimension(width, padding.left, padding.right)',
      },

      graphHeight: {
        readOnly: true,
        type: Number,
        computed: 'getGraphDimension(height, padding.top, padding.bottom)',
      },

    };
  }

  user: number;
  system: number;
  private numDataPoints: number;
  private dataRefreshPerSecond: number;
  private framesPerSecond: number;
  private frameDuration: number;
  private width: number;
  private height: number;
  private padding: ChartPadding;
  private graphWidth: number;
  private graphHeight: number;
  // Helper function to map range of x coordinates to graph width.
  private xAxisScaleFn: d3.ScaleLinear<number, number> = d3.scaleLinear();
  // Helper function to map range of y coordinates to graph height.
  private yAxisScaleFn: d3.ScaleLinear<number, number> = d3.scaleLinear();
  private data: Array<{user: number, system: number}> = [];
  // DOMHighResTimeStamp of last graph render.
  private lastRender: number = 0;
  // Current render frame out of this.framesPerSecond.
  private currentFrame: number = 0;
  // Y-Values where we should mark ticks for the y-axis on the left.
  private yAxisTicks: number[] = [0, 25, 50, 75, 100];

  constructor() {
    super();

    // Initialize the data array with data outside the chart boundary.
    // Note that with side nav DOM manipulation, created() isn't guaranteed to
    // be called only once.
    this.data = [];
    for (let i = 0; i < this.numDataPoints; ++i) {
      this.data.push({user: -1, system: -1});
    }
  }

  static get observers(): string[] {
    return ['setScaling(graphWidth)'];
  }

  override ready(): void {
    super.ready();
    this.setScaling();
    this.initializeChart();
    window.addEventListener('resize', () => this.updateChartWidth());

    // Set the initial chart width.
    this.updateChartWidth();
  }

  private updateChartWidth(): void {
    // parseFloat() is used to convert the string returned by
    // getComputedStyleValue() into a number ("642px" --> 642).
    const width = parseFloat(
        window.getComputedStyle(this).getPropertyValue('--chart-width-nav'));
    if (!isNaN(width)) {
      this.width = width;
    }
  }

  /**
   * Calculate the duration of each frame in milliseconds.
   */
  protected getFrameDuration(): number {
    assert(this.dataRefreshPerSecond > 0);
    assert(this.framesPerSecond > 0);
    assert(this.framesPerSecond % this.dataRefreshPerSecond === 0);
    return 1000 / (this.framesPerSecond / this.dataRefreshPerSecond);
  }

  /**
   * Get actual graph dimensions after accounting for margins.
   */
  private getGraphDimension(base: number, ...margins: number[]): number {
    return margins.reduce(((acc, margin) => acc - margin), base);
  }

  /**
   * Sets scaling functions that convert data -> svg coordinates.
   */
  private setScaling(): void {
    // Map y-values [0, 100] to [graphHeight, 0] inverse linearly.
    // Data value of 0 will map to graphHeight, 100 maps to 0.
    this.yAxisScaleFn =
        d3.scaleLinear().domain([0, 100]).range([this.graphHeight, 0]);

    // Map x-values [0, numDataPoints - 3] to [0, graphWidth] linearly.
    // Data value of 0 maps to 0, and (numDataPoints - 2) maps to graphWidth.
    // numDataPoints is subtracted since 1) data array is zero based, and
    // 2) to smooth out the curve function.
    this.xAxisScaleFn =
        d3.scaleLinear().domain([0, this.numDataPoints - 2]).range([
          0,
          this.graphWidth,
        ]);

    // Draw the y-axis legend and also draw the horizontal gridlines by
    // reversing the ticks back into the chart body.
    const chartGroup: d3.Selection<Element|null, unknown, null, undefined>|any =
        d3.select(this.shadowRoot!.querySelector('#chartGroup'));
    assert(chartGroup);
    chartGroup.select('#gridLines')
        .call(
            d3.axisLeft(this.yAxisScaleFn)
                .tickValues(this.yAxisTicks)
                .tickFormat((y) => this.getPercentageLabel((y as number)))
                .tickPadding(this.padding.tick)
                .tickSize(-this.graphWidth),  // Extend the ticks into the
                                              // entire graph as gridlines.
        );
  }

  private initializeChart(): void {
    const chartGroup: d3.Selection<Element|null, unknown, null, undefined> =
        d3.select(this.shadowRoot!.querySelector('#chartGroup'));
    assert(chartGroup);

    // Position chartGroup inside the margin.
    chartGroup.attr(
        'transform', `translate(${this.padding.left},${this.padding.top})`);

    const plotGroup: d3.Selection<Element|null, unknown, null, undefined> =
        d3.select(this.shadowRoot!.querySelector('#plotGroup'));
    assert(plotGroup);

    // Feed data array to the plot group.
    plotGroup.datum(this.data);

    // Select each area and configure the transition for animation.
    // d3.transition API @ https://github.com/d3/d3-transition#d3-transition.
    // d3.easing API @ https://github.com/d3/d3-ease#api-reference.
    plotGroup.select('.user-area')
        .transition()
        .duration(this.frameDuration)
        .ease((t: number) => +t);  // Linear transition
    plotGroup.select('.system-area')
        .transition()
        .duration(this.frameDuration)
        .ease((t: number) => +t);  // Linear transition

    // Draw initial data and kick off the rendering process.
    this.getDataSnapshotAndRedraw();
    this.render(/*timeStamp=*/ 0);
  }

  private getAreaDefinition(areaClass: string):
      d3.ValueFn<SVGPathElement, any, any> {
    return d3
        .area()
        // Take the index of each data as x values.
        .x((_, i) => this.xAxisScaleFn(i))
        // Bottom coordinates of each area. System area extends down to -1
        // instead of 0 to avoid the area border from showing up.
        .y0((data: any) => this?.yAxisScaleFn(
                areaClass === 'system-area' ? -1 : data?.system))
        // Top coordinates of each area.
        .y1((data: any) => this?.yAxisScaleFn(
                areaClass === 'system-area' ? data?.system :
                                              data?.system + data?.user));
  }

  /**
   * Takes a snapshot of current CPU readings and appends to the data array for
   * redrawing. This method is called after each transition cycle.
   */
  private getDataSnapshotAndRedraw(): void {
    this.data.push({user: this.user, system: this.system});
    this.data.shift();

    const userArea: d3.Selection<Element|null, unknown, null, undefined>|any =
        this.shadowRoot!.querySelector(`path.user-area`);
    assert(userArea);
    const systemArea: d3.Selection<Element|null, unknown, null, undefined>|any =
        this.shadowRoot!.querySelector(`path.system-area`);
    assert(systemArea);
    d3.select(userArea).attr('d', this.getAreaDefinition('user-area'));
    d3.select(systemArea).attr('d', this.getAreaDefinition('system-area'));
  }


  private render(timeStamp: number): void {
    // Re-render only when this.frameDuration has passed since last render.
    // If we acquire the animation frame before this, do nothing.
    if (timeStamp - this.lastRender > this.frameDuration) {
      this.lastRender = performance.now();

      // Get new data and redraw on each cycle.
      const framesPerCycle = this.framesPerSecond / this.dataRefreshPerSecond;
      if (this.currentFrame === framesPerCycle) {
        this.currentFrame = 0;
        this.getDataSnapshotAndRedraw();
      }

      const userArea: d3.Selection<Element|null, unknown, null, undefined>|any =
          this.shadowRoot!.querySelector(`path.user-area`);
      assert(userArea);
      const systemArea: d3.Selection<Element|null, unknown, null, undefined>|
          any = this.shadowRoot!.querySelector(`path.system-area`);
      assert(systemArea);

      // Calculate the new position. Use this.currentFrame + 1 since on frame
      // 0, it is already at position 0.
      const pos = -1 * ((this.currentFrame + 1) / framesPerCycle);

      // Slide it to the left
      d3.select(userArea).attr(
          'transform', 'translate(' + this.xAxisScaleFn(pos) + ',0)');
      d3.select(systemArea)
          .attr('transform', 'translate(' + this.xAxisScaleFn(pos) + ',0)');
      this.currentFrame++;
    }

    // Request another frame.
    requestAnimationFrame((timeStamp) => this.render(timeStamp));
  }

  private getPercentageLabel(value: number): string {
    return loadTimeData.getStringF('percentageLabel', value);
  }

  getPaddingForTesting(): ChartPadding {
    return this.padding;
  }

  getFrameDurationForTesting(): number {
    return this.frameDuration;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [RealtimeCpuChartElement.is]: RealtimeCpuChartElement;
  }
}

customElements.define(RealtimeCpuChartElement.is, RealtimeCpuChartElement);
