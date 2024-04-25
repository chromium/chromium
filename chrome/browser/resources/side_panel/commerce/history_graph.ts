// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/d3/d3.min.js';

import type {PricePoint} from '//resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './history_graph.html.js';

const LINE_AREA_HEIGHT_PX = 92;
const GRAPH_BUBBLE_BOTTOM_MARGIN_PX = 8;
const MIN_MONTH_LABEL_WIDTH_PX = 20;
const LABEL_FONT_WEIGHT = '400';
const LABEL_FONT_WEIGHT_BOLD = '700';
const CIRCLE_RADIUS_PX = 4;
const TICK_COUNT_Y = 4;

enum CssClass {
  AXIS = 'axis',
  BUBBLE = 'bubble',
  CIRCLE = 'circle',
  DASH_LINE = 'dash-line',
  DIVIDER = 'divider',
  PATH = 'path',
  TICK = 'tick',
  TOOLTIP_TEXT = 'tooltip-text',
}

export interface ShoppingInsightsHistoryGraphElement {
  $: {
    historyGraph: HTMLElement,
  };
}

// Element that controls the history graph.
export class ShoppingInsightsHistoryGraphElement extends PolymerElement {
  static get is() {
    return 'shopping-insights-history-graph';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      data: Array,
      locale: String,
      currency: String,
    };
  }

  data: PricePoint[];
  locale: string;
  currency: string;
  private points: Array<{date: Date, price: number}>;
  private isGraphInteracted_: boolean = false;
  private currentPricePointIndex_?: number;
  private resizeObserver_: ResizeObserver;
  private currentWidth_: number;
  private graphSvg_: any;
  private dateTopMarginPx_ = 12;
  private priceRightMarginPx_ = 8;
  private bubbleHorizontalPaddingPx_ = 6;
  private bubbleTopPaddingPx_ = 3;
  private bubbleBottomPaddingPx_ = 2;
  private bubbleCornerRadiusPx_ = 3;

  override connectedCallback() {
    super.connectedCallback();

    this.points = this.data.map(
        d => ({date: this.stringToDate_(d.date), price: d.price}));

    this.drawHistoryGraph_();

    this.currentWidth_ = this.$.historyGraph.offsetWidth;
    this.resizeObserver_ = new ResizeObserver(this.onResize_.bind(this));
    this.resizeObserver_.observe(this.$.historyGraph);
  }

  override disconnectedCallback() {
    this.resizeObserver_.disconnect();
  }

  private stringToDate_(s: string): Date {
    // When compiled, new Date('yyyy-mm-dd') does not return a valid Date
    // object. Using Date(year, monthIndex, day) protects against that.
    const [yearStr, monthStr, dayStr] = s.split('-');
    const year: number = parseInt(yearStr, 10);
    const month: number = parseInt(monthStr, 10);
    const day: number = parseInt(dayStr, 10);
    return new Date(year, month - 1, day);
  }

  private onResize_() {
    if (this.$.historyGraph.offsetWidth !== this.currentWidth_) {
      this.currentWidth_ = this.$.historyGraph.offsetWidth;
      this.graphSvg_.remove();
      this.drawHistoryGraph_();
    }
  }

  private getTooltipText_(i: number): string {
    let formattedDate = d3.timeFormat('%b %-d')(this.points[i].date);

    const previousDay = new Date();
    previousDay.setDate(previousDay.getDate() - 1);
    if (i === this.points.length - 1 &&
        this.points[i].date.getDate() === previousDay.getDate() &&
        this.points[i].date.getMonth() === previousDay.getMonth()) {
      formattedDate = loadTimeData.getString('yesterday');
    }

    const formattedPrice = this.data[i].formattedPrice;
    return `${formattedPrice}\n ${formattedDate}`;
  }

  private drawHistoryGraph_() {
    // Calculate graph size.
    const tooltipHeight = this.getTooltipHeight_();

    const [ticks, formattedTicks] = this.getAxisTicksY_();
    const [maxLabelWidth, labelHeight] =
        this.getLabelSize_(formattedTicks[formattedTicks.length - 1]);

    const graphMarginTopPx = GRAPH_BUBBLE_BOTTOM_MARGIN_PX +
        this.bubbleTopPaddingPx_ + this.bubbleBottomPaddingPx_ + tooltipHeight;
    const graphMarginBottomPx = this.dateTopMarginPx_ + labelHeight;
    const graphHeightPx =
        LINE_AREA_HEIGHT_PX + graphMarginTopPx + graphMarginBottomPx;
    const graphMarginLeftPx = maxLabelWidth + this.priceRightMarginPx_;
    const graphMarginRightPx = CIRCLE_RADIUS_PX + 1;

    const svg = d3.select(this.$.historyGraph)
                    .append('svg')
                    .attr('width', '100%')
                    .attr('height', graphHeightPx)
                    .attr('background-color', 'transparent');
    this.graphSvg_ = svg;
    const node = svg.node();
    assert(node);
    const graphWidthPx = node.getBoundingClientRect().width;

    // Set up scales and axes.
    const xRange = [graphMarginLeftPx, graphWidthPx - graphMarginRightPx];
    const yRange = [graphHeightPx - graphMarginBottomPx, graphMarginTopPx];

    const xDomain =
        [this.points[0].date, this.points[this.points.length - 1].date];
    const yDomain = [ticks[0], ticks[ticks.length - 1]];

    const xScale = d3.scaleTime(xDomain, xRange);
    const yScale = d3.scaleLinear(yDomain, yRange);

    const xAxis = d3.axisBottom<Date>(xScale)
                      .tickValues(this.getAxisTicksX_(xScale))
                      .tickFormat(d3.timeFormat('%b'))
                      .tickSize(0)
                      .tickPadding(this.dateTopMarginPx_);
    const yAxis = d3.axisLeft(yScale)
                      .tickValues(ticks)
                      .tickFormat((_, i) => formattedTicks[i])
                      .tickSize(0)
                      .tickPadding(this.priceRightMarginPx_);

    // Set up the line.
    const line = d3.line<{date: Date, price: number}>()
                     .curve(d3.curveLinear)
                     .x(d => xScale(d.date))
                     .y(d => yScale(d.price));

    // Add the X axis.
    svg.append('g')
        .attr(
            'transform', `translate(0,${graphHeightPx - graphMarginBottomPx})`)
        .call(xAxis)
        .call(g => g.select('.domain').classed(CssClass.AXIS, true))
        .call(
            g => g.selectAll('text')
                     .classed(CssClass.TICK, true)
                     .attr('aria-hidden', 'true'));

    // Add the Y axis.
    svg.append('g')
        .attr('transform', `translate(${graphMarginLeftPx},0)`)
        .call(yAxis)
        .call(g => g.select('.domain').remove())
        .call(
            g => g.selectAll('.tick line')
                     .clone()
                     .attr(
                         'x2',
                         graphWidthPx - graphMarginLeftPx - graphMarginRightPx)
                     .classed(CssClass.DIVIDER, true))
        .call(
            g => g.selectAll('text')
                     .classed(CssClass.TICK, true)
                     .attr('aria-hidden', 'true'));

    // Add the line.
    svg.append('path')
        .datum(this.points)
        .attr('d', line)
        .classed(CssClass.PATH, true);

    // Set up bubble and mouse listeners.
    const verticalLine =
        svg.append('line')
            .attr(
                'y1',
                this.bubbleTopPaddingPx_ + this.bubbleBottomPaddingPx_ +
                    tooltipHeight)
            .attr('y2', graphHeightPx - graphMarginBottomPx)
            .attr('opacity', 0)
            .classed(CssClass.DASH_LINE, true);

    const circle = svg.append('circle')
                       .attr('r', CIRCLE_RADIUS_PX)
                       .attr('opacity', 0)
                       .classed(CssClass.CIRCLE, true);

    this.bubbleCornerRadiusPx_ =
        (this.bubbleTopPaddingPx_ + this.bubbleBottomPaddingPx_ +
        tooltipHeight) /
        2;
    const bubble = svg.append('rect')
                       .attr('opacity', 0)
                       .attr('y', 0)
                       .attr(
                           'height',
                           this.bubbleTopPaddingPx_ +
                               this.bubbleBottomPaddingPx_ + tooltipHeight)
                       .attr('rx', this.bubbleCornerRadiusPx_)
                       .attr('ry', this.bubbleCornerRadiusPx_)
                       .classed(CssClass.BUBBLE, true);

    const tooltip = svg.append('text')
                        .attr('y', this.bubbleTopPaddingPx_ + tooltipHeight / 2)
                        .attr('dominant-baseline', 'middle')
                        .attr('opacity', 0)
                        .attr('aria-hidden', 'true');

    const initialIndex = this.currentPricePointIndex_ == null ?
        this.points.length - 1 :
        this.currentPricePointIndex_;
    this.showTooltip_(
        verticalLine, circle, bubble, tooltip, initialIndex,
        xScale(this.points[initialIndex].date),
        yScale(this.points[initialIndex].price), graphWidthPx);

    this.$.historyGraph.addEventListener('pointermove', (e: PointerEvent) => {
      const mouseX = e.offsetX;
      const mouseY = e.offsetY;
      if (mouseX < xRange[0] || mouseX > xRange[1] || mouseY < yRange[1] ||
          mouseY > yRange[0]) {
        return;
      }
      if (!this.isGraphInteracted_) {
        chrome.metricsPrivate.recordUserAction(
            'Commerce.PriceInsights.HistoryGraphInteraction');
        this.isGraphInteracted_ = true;
      }
      const nearestIndex =
          this.points.reduce((minIndex, _, currentIndex, array) => {
            return Math.abs(mouseX - xScale(array[currentIndex].date)) <
                    Math.abs(mouseX - xScale(array[minIndex].date)) ?
                currentIndex :
                minIndex;
          }, 0);
      this.showTooltip_(
          verticalLine, circle, bubble, tooltip, nearestIndex,
          xScale(this.points[nearestIndex].date),
          yScale(this.points[nearestIndex].price), graphWidthPx);
    });

    this.$.historyGraph.addEventListener('keydown', (e: KeyboardEvent) => {
      if (this.currentPricePointIndex_ != null) {
        let nextIndex = -1;

        if (e.key === 'ArrowLeft') {
          nextIndex = this.currentPricePointIndex_ - 1;
        } else if (e.key === 'ArrowRight') {
          nextIndex = this.currentPricePointIndex_ + 1;
        }

        if (nextIndex >= 0 && nextIndex <= this.points.length - 1) {
          this.showTooltip_(
              verticalLine, circle, bubble, tooltip, nextIndex,
              xScale(this.points[nextIndex].date),
              yScale(this.points[nextIndex].price), graphWidthPx);
        }
      }
    });

    this.$.historyGraph.setAttribute(
        'aria-label',
        loadTimeData.getString('historyTitle') +
            this.getTooltipText_(initialIndex) +
            loadTimeData.getString('historyGraphAccessibility'));
  }

  // Calculate y-axis ticks.
  // TODO(b:289435365): For now we use a rough method to generate the axis
  // ticks. After getting the approval to use backend method, we should switch
  // to it.
  private getAxisTicksY_(): [number[], string[]] {
    const ticks: number[] = [];
    const formattedTicks: string[] = [];

    let minPrice = this.points.reduce((min, value) => {
      return Math.min(min, value.price);
    }, this.points[0].price);
    let maxPrice = this.points.reduce((max, value) => {
      return Math.max(max, value.price);
    }, this.points[0].price);

    // To ensure that the Y-axis doesn't reflect trivial changes and that the
    // line is in the middle of the graph, apply a padding max(median price /
    // 10, $1) to the minPrice and maxPrice.
    const medianPrice = ([...this.points].sort(
        (a, b) => a.price - b.price))[Math.floor(this.points.length / 2)]
                            .price;
    const padding = Math.max(medianPrice / 10, 1);
    minPrice = Math.max(minPrice - padding, 0);
    maxPrice = maxPrice + padding;

    const valueRange = maxPrice - minPrice;
    let tickInterval = valueRange / (TICK_COUNT_Y - 1);

    // Ensure the tick interval is a multiple of below values to improve the
    // readability. Bigger values are used when possible.
    const multipliers = [100, 50, 20, 10, 5, 2, 1];

    let multiplier = 1;
    for (const tempMultiplier of multipliers) {
      if (tickInterval >= 2 * tempMultiplier) {
        multiplier = tempMultiplier;
        break;
      }
    }
    tickInterval = Math.ceil(tickInterval / multiplier) * multiplier;

    let tickLow =
        minPrice - (tickInterval * (TICK_COUNT_Y - 1) - valueRange) / 2;
    tickLow = Math.floor(tickLow / multiplier) * multiplier;
    tickLow = Math.max(tickLow, 0);

    tickInterval =
        Math.ceil((maxPrice - tickLow) / (TICK_COUNT_Y - 1) / multiplier) *
        multiplier;

    const formatter = new Intl.NumberFormat(this.locale, {
      style: 'currency',
      currency: this.currency,
      minimumFractionDigits: 0,
      maximumFractionDigits: 0,
    });

    // Populate results.
    for (let i = 0; i < TICK_COUNT_Y; i++) {
      const tickValue = tickLow + i * tickInterval;
      ticks.push(tickValue);
      formattedTicks.push(formatter.format(tickValue));
    }
    return [ticks, formattedTicks];
  }

  // Calculate x-axis ticks.
  private getAxisTicksX_(xScale: (d: Date) => number): Date[] {
    const ticks: Date[] = [];
    const indicesByMonth: number[][] = Array.from({length: 12}, () => []);
    for (let i = 0; i < this.points.length; i++) {
      const month = this.points[i].date.getMonth();
      indicesByMonth[month].push(i);
    }
    for (const indices of indicesByMonth) {
      if (indices.length) {
        const x1 = xScale(this.points[indices[0]].date);
        const x2 = xScale(this.points[indices[indices.length - 1]].date);
        const width = x2 - x1;
        if (width < MIN_MONTH_LABEL_WIDTH_PX) {
          continue;
        }
        ticks.push(this.points[indices[Math.floor(indices.length / 2)]].date);
      }
    }
    return ticks;
  }

  // Estimates the width and height of label text before it is rendered.
  private getLabelSize_(text: string): [number, number] {
    const textElement = document.createElement('span');
    textElement.style.opacity = '0';
    textElement.textContent = text;
    textElement.classList.add(CssClass.TICK);


    this.$.historyGraph.appendChild(textElement);
    const labelWidth = textElement.offsetWidth;
    const labelHeight = textElement.offsetHeight;
    textElement.remove();
    return [labelWidth, labelHeight];
  }

  // Estimates the height of tooltip text before it is rendered.
  private getTooltipHeight_(): number {
    const textElement = document.createElement('span');
    textElement.style.opacity = '0';
    textElement.textContent = this.data[0].formattedPrice;
    textElement.classList.add(CssClass.TOOLTIP_TEXT);

    this.$.historyGraph.appendChild(textElement);
    const labelHeight = textElement.offsetHeight;
    textElement.remove();
    return labelHeight;
  }

  private showTooltip_(
      verticalLine: d3.Selection<SVGLineElement, unknown, null, undefined>,
      circle: d3.Selection<SVGCircleElement, unknown, null, undefined>,
      bubble: d3.Selection<SVGRectElement, unknown, null, undefined>,
      tooltip: d3.Selection<SVGTextElement, unknown, null, undefined>,
      index: number, pointX: number, pointY: number, graphWidth: number) {
    tooltip.join('text').call(
        (text: d3.Selection<SVGTextElement, unknown, null, undefined>) =>
            text.selectAll('tspan')
                .data(`${this.getTooltipText_(index)}`.split(/\n/))
                .join('tspan')
                .classed(CssClass.TOOLTIP_TEXT, true)
                .style(
                    'font-weight',
                    (d: string) =>
                        d.includes(loadTimeData.getString('yesterday')) ?
                        LABEL_FONT_WEIGHT_BOLD :
                        LABEL_FONT_WEIGHT)
                .style('dominant-baseline', 'middle')
                .text((d: string) => d));

    const tooltipNode = tooltip.node();
    assert(tooltipNode);

    verticalLine.attr('x1', pointX).attr('x2', pointX).attr('opacity', 1);
    circle.attr('cx', pointX).attr('cy', pointY).attr('opacity', 1);

    const tooltipWidth = tooltipNode.getBBox().width;
    const bubbleWidth = tooltipWidth + 2 * this.bubbleHorizontalPaddingPx_;
    let bubbleStart = pointX - bubbleWidth / 2;
    if (bubbleStart < 0) {
      bubbleStart = 0;
    }
    if (bubbleStart + bubbleWidth > graphWidth) {
      bubbleStart = graphWidth - bubbleWidth;
    }
    tooltip.attr('x', bubbleStart + bubbleWidth / 2).attr('opacity', 1);
    bubble.attr('x', bubbleStart).attr('width', bubbleWidth).attr('opacity', 1);

    this.$.historyGraph.setAttribute('aria-label', this.getTooltipText_(index));
    this.currentPricePointIndex_ = index;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'shopping-insights-history-graph': ShoppingInsightsHistoryGraphElement;
  }
}

customElements.define(
    ShoppingInsightsHistoryGraphElement.is,
    ShoppingInsightsHistoryGraphElement);
