// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import type {Signals} from '../omnibox_internals.mojom-webui.js';
import {clamp, signalNames} from '../omnibox_util.js';

import type {MlBrowserProxy} from './ml_browser_proxy.js';
/* eslint-disable-next-line @typescript-eslint/ban-ts-comment */
// @ts-ignore:next-line
import sheet from './ml_chart.css' with {type : 'css'};
import {getTemplate} from './ml_chart.html.js';

// Represents a line of text when drawing multiline text onto the canvas.
interface TextLine {
  text: string;
  color: string;
  fontSize: number;
  bold: boolean;
}

// Represents a single point when drawing a line plot onto the canvas.
interface PlotPoint {
  position: Vector;
  label: TextLine[];
}

// Represents a line plot to be drawn onto the canvas.
interface Plot {
  points: PlotPoint[];
  label: string;
  color: string;
  xAxisLabel: string;
  xAxisOffset: number;
  xAxisScale: number;
}

// Helps do vector math.
class Vector {
  x: number;
  y: number;

  constructor(x: number = 0, y: number = x) {
    this.x = x;
    this.y = y;
  }

  get array(): [number, number] {
    return [this.x, this.y];
  }

  setX(x: number) {
    return new Vector(x, this.y);
  }

  setY(y: number) {
    return new Vector(this.x, y);
  }

  add(v: Vector) {
    return new Vector(this.x + v.x, this.y + v.y);
  }

  subtract(v: Vector) {
    return this.add(v.negate());
  }

  pointwiseMultiply(v: Vector) {
    return new Vector(this.x * v.x, this.y * v.y);
  }

  pointwiseDivide(v: Vector) {
    return new Vector(this.x / v.x, this.y / v.y);
  }

  scale(scaler: number) {
    return this.pointwiseMultiply(new Vector(scaler));
  }

  negate() {
    return this.scale(-1);
  }

  // Useful because the canvas coordinates has y=0 at the top, but grid
  // coordinates has y=0 at the bottom.
  invertY(maxY: number) {
    return this.setY(maxY - this.y);
  }

  magnitudeSqr() {
    return this.x ** 2 + this.y ** 2;
  }

  clamp(min: Vector, max: Vector) {
    return new Vector(clamp(this.x, min.x, max.x), clamp(this.y, min.y, max.y));
  }

  // Transforms from coordinate system to another. Canvas <-> grid coordinates.
  transform(
      oldOrigin: Vector, oldSize: Vector, newOrigin: Vector, newSize: Vector) {
    return this.subtract(oldOrigin)
        .transformScale(oldSize, newSize)
        .add(newOrigin);
  }

  transformScale(oldSize: Vector, newSize: Vector) {
    return this.pointwiseDivide(oldSize).pointwiseMultiply(newSize);
  }
}

export class MlChartElement extends CustomElement {
  private mlBrowserProxy_: MlBrowserProxy;
  private signals_: Signals;
  private plots: Plot[] = [];

  private context: CanvasRenderingContext2D;

  private readonly clearColor = this.getCssProperty('--theme');
  private readonly primaryColor = this.getCssProperty('--text');

  private canvasSize: Vector;
  private readonly axisPadding =
      new Vector(50);        // Padding between canvas border and axes lines.
  private gridMin: Vector;   // The grid coordinate of the axes origin.
  private gridSize: Vector;  // The grid lengths of the axes.

  private mouseDown: boolean = false;  // Whether a mouse button is down.
  private mousePosition: Vector;       // Canvas coordinates of the mouse.

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.shadowRoot!.adoptedStyleSheets = [sheet];
  }

  connectedCallback() {
    const canvas = this.getRequiredElement('canvas');
    this.canvasSize = new Vector(canvas.width, canvas.height);
    this.context = canvas.getContext('2d')!;
    canvas.addEventListener(
        'mousemove',
        e => this.onMouseMove(e.buttons > 0, new Vector(e.offsetX, e.offsetY)));
    canvas.addEventListener('wheel', e => {
      e.preventDefault();
      this.onMouseWheel(new Vector(e.offsetX, e.offsetY), Math.sign(e.deltaY));
    });
  }

  set mlBrowserProxy(mlBrowserProxy: MlBrowserProxy) {
    this.mlBrowserProxy_ = mlBrowserProxy;
  }

  set signals(signals: Signals) {
    this.plots = [];
    this.clear();

    this.signals_ = signals;

    // Set grid [-15, 0] and [15, 1] to line up with the axes' starts and ends.
    this.gridSize = new Vector(30, 1)
                        .pointwiseMultiply(this.canvasSize)
                        .pointwiseDivide(this.canvasSize.subtract(
                            this.axisPadding.scale(2)));
    // Subtract half the gridSize from the grid center.
    this.gridMin = new Vector(0, .5).subtract(this.gridSize.scale(.5));

    this.createPlots();
  }

  private async createPlots() {
    if (!this.signals_ || !this.mlBrowserProxy_) {
      return;
    }

    // Only graph the 1st 4 signals, since those happen to be the one's we're
    // interested in most often.
    const chartSignalNames = signalNames.slice(0, 4);

    // If there are more than `colors.length` plots, colors will be repeated.
    const colors = [
      this.getColor(0),    // red
      this.getColor(120),  // green
      this.getColor(240),  // blue
      this.getColor(300),  // pink
    ];

    const minX = Math.floor(this.gridMin.x);
    const maxX = Math.ceil(this.gridMin.add(this.gridSize).x);
    const xValues = [...Array(maxX - minX + 1)].map((_, j) => minX + j);

    interface MlRequest {
      x: number;
      scale: number;
      modifiedSignals: Signals;
      score: number;
    }
    const mlRequestPromises: Array<Array<Promise<MlRequest>>> =
        chartSignalNames.map(signalName => {
          const signal = this.signals_[signalName];
          // For signals such as elapsedTimeLastVisitSecs, it's sometimes
          // more useful to visualize on a zoomed-out x-axis. When their values
          // are small though, we still want to use the normal scale so we can
          // see patterns like score humps that tend to be in the 1-100 range.
          // So zoom-out based on the signal value; when the signal is [0-99]
          // scale is 1; when the signal is [100, 999], scale by 10; etc.
          const scale =
              (typeof signal === 'number' || typeof signal === 'bigint') ?
              10 ** Math.max(Math.floor(Math.log10(Number(signal)) - 1), 0) :
              1;
          return xValues
              .map((x): [number, number] => [x, Number(signal) + x * scale])
              .filter(([_, modifiedSignal]) => modifiedSignal > 0)
              .map(async([x, modifiedSignal]): Promise<MlRequest> => {
                const modifiedSignals = {
                  ...this.signals_,
                  [signalName]: modifiedSignal,
                };
                const score =
                    await this.mlBrowserProxy_.makeMlRequest(modifiedSignals);
                return {x, scale, modifiedSignals, score};
              });
        });
    const mlRequests: MlRequest[][] = await Promise.all(
        mlRequestPromises.map(arrayOfPromises => Promise.all(arrayOfPromises)));

    this.plots = chartSignalNames.map((signalName, i): Plot => {
      return {
        points: mlRequests[i]!.map(
            (mlRequest):
                PlotPoint => {
                  return {
                    position: new Vector(mlRequest.x, mlRequest.score),
                    label: [
                      ...chartSignalNames.map(
                          (signalName2, k): TextLine => ({
                            text: `${signalName2}: ${
                                Number(mlRequest.modifiedSignals[signalName2]!)
                                    .toLocaleString('en-US')}`,
                            color: colors[k % colors.length]!,
                            fontSize: 12,
                            bold: signalName2 === signalName,
                          })),
                      {
                        text: `Score: ${mlRequest.score.toFixed(3)}`,
                        color: this.primaryColor,
                        fontSize: 12,
                        bold: true,
                      },
                    ],
                  };
                }),
        label: signalName,
        color: colors[i % colors.length]!,
        xAxisLabel: signalName,
        xAxisOffset: Number(this.signals_[signalName]),
        xAxisScale: mlRequests[i]![0]?.scale || 1,
      };
    });

    this.draw();
  }

  private onMouseMove(mouseDown: boolean, position: Vector) {
    if (!this.plots.length) {
      return;
    }
    // If dragging the mouse, pan the grid.
    if (this.mouseDown && mouseDown) {
      this.gridMin = this.gridMin.subtract(
          this.invWh(position.subtract(this.mousePosition)));
      this.createPlots();
    }
    this.mouseDown = mouseDown;
    this.mousePosition = position;
    this.draw(position);
  }

  private onMouseWheel(position: Vector, zoom: number) {
    if (!this.plots.length) {
      return;
    }
    // Pan towards the mouse.
    const weight = .1;
    const oldGridCenter = this.gridMin.add(this.gridSize.scale(.5));
    const newGridCenter = this.invXy(position).scale(weight).add(
        oldGridCenter.scale((1 - weight)));
    // Zoom in/out by 15%.
    this.gridSize = this.gridSize.scale(1 + .15 * zoom);
    this.gridMin = newGridCenter.subtract(this.gridSize.scale(.5));
    this.draw(position);
    this.createPlots();
  }

  private draw(mouse: Vector|null = null) {
    this.clear();
    if (!this.plots.length) {
      return;
    }

    // Find which plot, if any, the mouse is hovering nearest.
    let closestDistance = 900;  // If the mouse is within 30px.
    let closestPlot: Plot|null = null;
    let closestPoint: PlotPoint|null = null;
    if (mouse) {
      this.plots.forEach(plot => plot.points.forEach(point => {
        const distance = this.xy(point.position).subtract(mouse).magnitudeSqr();
        if (distance < closestDistance) {
          closestDistance = distance;
          closestPlot = plot;
          closestPoint = point;
        }
      }));
    }
    // Typescript weirdness.
    if (closestPlot) {
      closestPlot = closestPlot as Plot;
    }

    // Draw the axes.
    const axisOrigin = this.axisPadding.invertY(this.canvasSize.y);
    const axisLength =
        this.canvasSize.subtract(this.axisPadding.scale(2)).invertY(0);
    const tickLength = new Vector(15);
    const labelOffset = new Vector(20);
    const nTicks = 5;
    const xAxisColor = closestPlot ? closestPlot.color : this.primaryColor;
    // Draw the axes ticks and tick labels.
    for (let i = 0; i <= nTicks; i++) {
      const tick = axisOrigin.add(axisLength.scale(i / nTicks));
      const tickGrid = this.invXy(tick);
      this.drawLine(
          tick.setY(axisOrigin.subtract(tickLength.scale(.5)).y),
          tickLength.setX(0), 1, xAxisColor);
      if (closestPlot) {
        const tickLabel =
            (tickGrid.x * closestPlot.xAxisScale + closestPlot!.xAxisOffset)
                .toLocaleString(
                    'en-US',
                    {minimumFractionDigits: 1, maximumFractionDigits: 1});
        this.drawText(
            tickLabel, tick.setY(axisOrigin.add(labelOffset).y), xAxisColor, 12,
            false, 'center', 'middle');
      }
      this.drawLine(
          tick.setX(axisOrigin.subtract(tickLength.scale(.5)).x),
          tickLength.setY(0), 1, this.primaryColor);
      this.drawText(
          tickGrid.y.toFixed(2), tick.setX(axisOrigin.subtract(labelOffset).x),
          this.primaryColor, 12, false, 'center', 'middle');
    }
    this.drawLine(axisOrigin, axisLength.setY(0), 2, xAxisColor);
    this.drawLine(axisOrigin, axisLength.setX(0), 2, this.primaryColor);

    // Draw the axes titles.
    if (closestPlot) {
      this.drawText(
          closestPlot!.xAxisLabel,
          axisOrigin.add(axisLength.scale(.5).setY(0))
              .add(labelOffset.scale(2).setX(0)),
          xAxisColor, 12, false, 'center', 'middle');
    }
    this.drawVertText(
        'Score',
        axisOrigin.add(axisLength.scale(.5).setX(0))
            .add(labelOffset.scale(-2).setY(0)),
        this.primaryColor, 12, false, 'center', 'middle');

    // Draw the plots.
    this.plots.forEach(plot => plot.points.forEach((point, i, points) => {
      if (i) {
        const prev = points[i - 1]!;
        this.drawLine(
            this.xy(prev.position),
            this.wh(point.position.subtract(prev.position)),
            plot === closestPlot ? 3 : 1, plot.color);
      }
    }));

    // Draw the original signal.
    const centerPosition = this.plots.flatMap(plot => plot.points)
                               .map(point => point.position)
                               .find(position => !position.x);
    if (centerPosition) {
      this.drawPoint(this.xy(centerPosition), 7, this.primaryColor);
    }

    // Draw the legend.
    this.drawMultilineText(
        this.plots.map(plot => ({
                         text: plot.label,
                         color: plot.color,
                         fontSize: 12,
                         bold: plot === closestPlot,
                       })),
        this.canvasSize.setY(0), this.clearColor, this.clearColor, 'right');

    // Draw the tooltip if the mouse is hovering near a plot.
    if (closestPlot) {
      this.drawPoint(this.xy(closestPoint!.position!), 7, closestPlot!.color);
      this.drawMultilineText(
          closestPoint!.label, mouse!.add(labelOffset), closestPlot!.color,
          this.clearColor, 'left');
    }
  }

  private clear() {
    this.drawRect(new Vector(), this.canvasSize, 0, this.clearColor);
  }

  // Draws a filled square centered at `xy` with side length `size`.
  private drawPoint(xy: Vector, size: number, color: string) {
    const sizeV = new Vector(size);
    this.drawRect(xy.subtract(sizeV.scale(.5)), sizeV, 0, color);
  }

  // Draws a line from `xy` to `xy+wh`. `lineWidth` is in canvas units (pixels).
  private drawLine(xy: Vector, wh: Vector, lineWidth: number, color: string) {
    this.context.lineWidth = lineWidth;
    this.context.strokeStyle = color;
    this.context.beginPath();
    this.context.moveTo(...xy.array);
    this.context.lineTo(...xy.add(wh).array);
    this.context.stroke();
  }

  // Draws a rect, either outline-only or filled depending on if `lineWidth` is
  // given. `lineWidth` is in canvas units (pixels).
  private drawRect(xy: Vector, wh: Vector, lineWidth: number, color: string) {
    if (lineWidth) {
      this.context.lineWidth = lineWidth;
      this.context.strokeStyle = color;
      this.context.strokeRect(...xy.array, ...wh.array);
    } else {
      this.context.fillStyle = color;
      this.context.fillRect(...xy.array, ...wh.array);
    }
  }

  // `fontSize` is in canvas units (pixels).
  private drawText(
      text: string, xy: Vector, color: string, fontSize: number, bold: boolean,
      horizAlign: CanvasTextAlign, vertAlign: CanvasTextBaseline) {
    this.context.fillStyle = color;
    this.setFont(fontSize, bold);
    this.context.textAlign = horizAlign;
    this.context.textBaseline = vertAlign;
    this.context.fillText(text, ...xy.array);
  }

  // Draws text rotated 90deg counter clockwise. `fontSize` is in canvas units
  // (pixels).
  private drawVertText(
      text: string, xy: Vector, color: string, fontSize: number, bold: boolean,
      horizAlign: CanvasTextAlign, vertAlign: CanvasTextBaseline) {
    this.context.translate(...xy.array);
    this.context.rotate(-Math.PI / 2);
    this.context.translate(...xy.negate().array);
    this.drawText(text, xy, color, fontSize, bold, horizAlign, vertAlign);
    this.context.resetTransform();
  }

  // Draws a rectangle background, then draws text over it. Each line of text
  // can have different font, color, and style. `outlineColor` and
  // `backgroundColor` affect the rectangle only. The rectangle dimensions are
  // auto-computed to fit the text. The position `xy` will be adjusted to ensure
  // all the text fits on the canvas if possible.
  private drawMultilineText(
      textLines: TextLine[], xy: Vector, outlineColor: string,
      backgroundColor: string, horizAlign: 'left'|'right') {
    const padding = 3;
    const lineWh: Array<[number, number]> = textLines.map(textLine => {
      this.setFont(textLine.fontSize, textLine.bold);
      const m = this.context.measureText(textLine.text);
      return [m.width, m.fontBoundingBoxAscent + m.fontBoundingBoxDescent];
    });
    const textWidth = Math.max(...lineWh.map(wh => wh[0]));
    const textHeights = lineWh.map(wh => wh[1]);
    const rectSize = new Vector(
        textWidth + padding * 2,
        textHeights.reduce((sum, height) => sum + height, 0) + padding * 2);

    xy = xy.clamp(new Vector(), this.canvasSize.subtract(rectSize));

    this.drawRect(xy, rectSize, 0, backgroundColor);
    this.drawRect(xy, rectSize, 1, outlineColor);
    if (horizAlign === 'right') {
      xy = xy.add(new Vector(textWidth, 0));
    }
    xy = xy.add(new Vector(padding));
    textLines.forEach((textLine, i) => {
      this.drawText(
          textLine.text, xy, textLine.color, textLine.fontSize, textLine.bold,
          horizAlign, 'top');
      xy.y += textHeights[i]!;
    });
  }

  // Converts grid coordinates to canvas coordinates. E.g. [1, 1] -> [600, 0].
  private xy(v: Vector) {
    return v.transform(
        this.gridMin, this.gridSize, this.canvasSize.setX(0),
        this.canvasSize.invertY(0));
  }

  // Converts grid distances to canvas distances. E.g. [1, 1] -> [600, 600].
  private wh(v: Vector): Vector {
    return v.transformScale(this.gridSize, this.canvasSize.invertY(0));
  }

  // Converts canvas coordinates to grid coordinates. E.g. [600, 600] -> [1, 0].
  private invXy(v: Vector): Vector {
    return v.transform(
        this.canvasSize.setX(0), this.canvasSize.invertY(0), this.gridMin,
        this.gridSize);
  }

  // Converts canvas distances to grid distances. E.g. [600, 600] -> [1, 1].
  private invWh(v: Vector): Vector {
    return v.transformScale(this.canvasSize.invertY(0), this.gridSize);
  }

  private setFont(fontSize: number, bold: boolean) {
    this.context.font = `${bold ? 'bold' : ''} ${fontSize}px arial`;
  }

  // Helper to read css variables like `var(--property)` defined in ml.css.
  private getCssProperty(property: string) {
    return getComputedStyle(this).getPropertyValue(property);
  }

  // Helper to get colors consistent with the colored texts defined in ml.css.
  private getColor(h: number) {
    return `hsl(${h}, 50%, ${this.getCssProperty('--color-lightness')})`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ml-chart': MlChartElement;
  }
}

customElements.define('ml-chart', MlChartElement);
