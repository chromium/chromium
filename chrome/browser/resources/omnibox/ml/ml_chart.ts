// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {Signals} from '../omnibox.mojom-webui.js';
import {clamp, signalNames} from '../omnibox_util.js';

import {MlBrowserProxy} from './ml_browser_proxy';
// @ts-ignore:next-line
import sheet from './ml_chart.css' assert {type : 'css'};
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
  private readonly nPoints = 29;  // Max number of points per plot.

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
    const canvas = this.getRequiredElement<HTMLCanvasElement>('canvas');
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
    this.setSignals(signals);
  }

  private async setSignals(signals: Signals) {
    this.clear();

    // Debounce 1s. E.g., if the user is using the up arrow key to increase a
    // signal from x to x+100, there's no need to redraw the plot 100 times
    // which will be laggy; wait until the user's done and draw the final plots.
    this.signals_ = signals;
    await new Promise(r => setTimeout(r, 1000));
    if (this.signals_ !== signals) {
      return;
    }

    // Set grid [0, 0] and [1, 1] to line up with the axes' starts and ends.
    this.gridSize = this.canvasSize.pointwiseDivide(
        this.canvasSize.subtract(this.axisPadding.scale(2)));
    this.gridMin = this.gridSize.subtract(new Vector(1)).scale(-.5);

    // If there are more than `colors.length` plots, colors will be repeated.
    const colors = [
      this.getColor(0),    // red
      this.getColor(120),  // green
      this.getColor(240),  // blue
      this.getColor(300),  // pink
    ];
    // To draw a plot, all except 1 signal is held constant constant, while that
    // 1 signal is tweaked from it's initial value by -/+nPoints. Modification
    // represents a tweak and its score.
    interface Modification {
      i: number;
      newValue: number;
      score: number;
    }
    const modificationSets: Array<{
      signalName: keyof Signals,
      modifications: Modification[],
    }> =
        (await Promise.all(signalNames.map(
             async signalName => ({
               signalName,
               modifications:
                   (await Promise.all([
                     ...Array(this.nPoints),
                   ].map(async (_, i) => {
                     const newValue = Number(signals[signalName]) + i -
                         (this.nPoints - 1) / 2;
                     if (newValue < 0) {
                       return;
                     }
                     const modifiedSignals = {
                       ...signals,
                       [signalName]: String(newValue),
                     };
                     const score = await this.mlBrowserProxy_.makeMlRequest(
                         modifiedSignals);
                     return {
                       i,
                       newValue,
                       score,
                     };
                   }))).filter(modification => modification) as Modification[],
             }))))
            .filter(modificationSet => {
              return modificationSet.modifications.length &&
                  // Filter out signals that did not affect the score.
                  modificationSet.modifications.some(
                      m =>
                          m!.score !== modificationSet.modifications[0]!.score);
            });

    this.plots = modificationSets.map(
        (modificationSet, i) => ({
          points: modificationSet.modifications.map(
              modification => ({
                position: new Vector(
                    modification.i / (this.nPoints - 1), modification.score),
                label: [
                  ...modificationSets
                      .map(modificationSet => modificationSet.signalName)
                      .map((signalName, j) => ({
                             text: [
                               signalName,
                               signalName === modificationSet.signalName ?
                                   modification.newValue :
                                   signals[signalName],
                             ].join(': '),
                             color: colors[j % colors.length]!,
                             fontSize: 12,
                             bold: signalName === modificationSet.signalName,
                           })),
                  {
                    text: `Score: ${modification.score.toFixed(3)}`,
                    color: this.primaryColor,
                    fontSize: 12,
                    bold: true,
                  },
                ],
              })),
          label: modificationSet.signalName,
          color: colors[i % colors.length]!,
          xAxisLabel: modificationSet.signalName,
          xAxisOffset: Number(signals[modificationSet.signalName]),
        }));

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
  }

  private async draw(mouse: Vector|null = null) {
    this.clear();
    if (!this.plots.length) {
      return;
    }

    // Find which plot, if any, the mouse is hovering nearest.
    let closestDistance = 0.003;
    let closestPlot: Plot|null = null;
    let closestPoint: PlotPoint|null = null;
    if (mouse) {
      const gridMouse = this.invXy(mouse);
      this.plots.forEach(plot => plot.points.forEach(point => {
        const distance = point.position.subtract(gridMouse).magnitudeSqr();
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
    const nTicks = 10;
    const xAxisColor = closestPlot ? closestPlot.color : this.primaryColor;
    // Draw the axes ticks and tick labels.
    for (let i = 0; i <= nTicks; i++) {
      const tick = axisOrigin.add(axisLength.scale(i / nTicks));
      const tickGrid = this.invXy(tick);
      this.drawLine(
          tick.setY(axisOrigin.subtract(tickLength.scale(.5)).y),
          tickLength.setX(0), 1, xAxisColor);
      if (closestPlot) {
        this.drawText(
            (closestPlot!.xAxisOffset + (tickGrid.x - .5) * (this.nPoints - 1))
                .toFixed(2),
            tick.setY(axisOrigin.add(labelOffset).y), xAxisColor, 12, false,
            'center', 'middle');
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
    if ((this.plots[0]?.points?.length || 0) - 1 > (this.nPoints - 1) / 2) {
      const centerPoint =
          this.plots[0]!
              .points[this.plots[0]!.points.length - (this.nPoints - 1) / 2 - 1]!
          ;
      this.drawPoint(this.xy(centerPoint.position), 7, this.primaryColor);
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
        this.canvasSize.invertY(-1));
  }

  // Converts grid distances to canvas distances. E.g. [1, 1] -> [600, 600].
  private wh(v: Vector): Vector {
    return v.transformScale(this.gridSize, this.canvasSize.invertY(0));
  }

  // Converts canvas coordinates to grid coordinates. E.g. [600, 600] -> [1, 0].
  private invXy(v: Vector): Vector {
    return v.transform(
        this.canvasSize.setX(0), this.canvasSize.invertY(-1), this.gridMin,
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

customElements.define('ml-chart', MlChartElement);
