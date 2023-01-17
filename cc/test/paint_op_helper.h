// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PAINT_OP_HELPER_H_
#define CC_TEST_PAINT_OP_HELPER_H_

#include <sstream>
#include <string>

#include "base/strings/stringprintf.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/skottie_wrapper.h"

namespace cc {

// A helper class to help with debugging PaintOp/PaintOpBuffer.
// Note that this file is primarily used for debugging. As such, it isn't
// typically a part of BUILD.gn (except for self-testing), so all of the
// implementation should be limited ot the header.
class PaintOpHelper {
 public:
  // Print optional/nullable wrapper types are using underlying types.
  template <typename T>
  static std::string ToString(const T* ptr) {
    return ptr ? ToString(*ptr) : "(nil)";
  }

  template <typename T>
  static std::string ToString(const sk_sp<T>& ptr) {
    return ptr ? ToString(*ptr) : "(nil)";
  }

  template <typename T>
  static std::string ToString(scoped_refptr<T> ptr) {
    return ptr ? ToString(*ptr) : "(nil)";
  }

  template <typename T>
  static std::string ToString(const absl::optional<T>& opt) {
    return opt.has_value() ? ToString(*opt) : "(nil)";
  }

  static std::string ToString(const PaintOp& base_op) {
    std::ostringstream str;
    str << std::boolalpha;
    switch (base_op.GetType()) {
      case PaintOpType::Annotate: {
        const auto& op = static_cast<const AnnotateOp&>(base_op);
        str << "AnnotateOp(type=" << ToString(op.annotation_type)
            << ", rect=" << ToString(op.rect) << ", data=" << ToString(op.data)
            << ")";
        break;
      }
      case PaintOpType::ClipPath: {
        const auto& op = static_cast<const ClipPathOp&>(base_op);
        str << "ClipPathOp(path=" << ToString(op.path)
            << ", op=" << ToString(op.op) << ", antialias=" << op.antialias
            << ", use_cache=" << (op.use_cache == UsePaintCache::kEnabled)
            << ")";
        break;
      }
      case PaintOpType::ClipRect: {
        const auto& op = static_cast<const ClipRectOp&>(base_op);
        str << "ClipRectOp(rect=" << ToString(op.rect)
            << ", op=" << ToString(op.op) << ", antialias=" << op.antialias
            << ")";
        break;
      }
      case PaintOpType::ClipRRect: {
        const auto& op = static_cast<const ClipRRectOp&>(base_op);
        str << "ClipRRectOp(rrect=" << ToString(op.rrect)
            << ", op=" << ToString(op.op) << ", antialias=" << op.antialias
            << ")";
        break;
      }
      case PaintOpType::Concat: {
        const auto& op = static_cast<const ConcatOp&>(base_op);
        str << "ConcatOp(matrix=" << ToString(op.matrix) << ")";
        break;
      }
      case PaintOpType::CustomData: {
        const auto& op = static_cast<const CustomDataOp&>(base_op);
        str << "CustomDataOp(id=" << ToString(op.id) << ")";
        break;
      }
      case PaintOpType::DrawColor: {
        const auto& op = static_cast<const DrawColorOp&>(base_op);
        str << "DrawColorOp(color=" << ToString(op.color)
            << ", mode=" << ToString(op.mode) << ")";
        break;
      }
      case PaintOpType::DrawDRRect: {
        const auto& op = static_cast<const DrawDRRectOp&>(base_op);
        str << "DrawDRRectOp(outer=" << ToString(op.outer)
            << ", inner=" << ToString(op.inner)
            << ", flags=" << ToString(op.flags) << ")";
        break;
      }
      case PaintOpType::DrawImage: {
        const auto& op = static_cast<const DrawImageOp&>(base_op);
        str << "DrawImageOp(image=" << ToString(op.image)
            << ", left=" << ToString(op.left) << ", top=" << ToString(op.top)
            << ", flags=" << ToString(op.flags) << ")";
        break;
      }
      case PaintOpType::DrawImageRect: {
        const auto& op = static_cast<const DrawImageRectOp&>(base_op);
        str << "DrawImageRectOp(image=" << ToString(op.image)
            << ", src=" << ToString(op.src) << ", dst=" << ToString(op.dst)
            << ", constraint=" << ToString(op.constraint)
            << ", flags=" << ToString(op.flags) << ")";
        break;
      }
      case PaintOpType::DrawIRect: {
        const auto& op = static_cast<const DrawIRectOp&>(base_op);
        str << "DrawIRectOp(rect=" << ToString(op.rect)
            << ", flags=" << ToString(op.flags) << ")";
        break;
      }
      case PaintOpType::DrawLine: {
        const auto& op = static_cast<const DrawLineOp&>(base_op);
        str << "DrawLineOp(x0=" << ToString(op.x0) << ", y0=" << ToString(op.y0)
            << ", x1=" << ToString(op.x1) << ", y1=" << ToString(op.y1)
            << ", flags=" << ToString(op.flags) << ")";
        break;
      }
      case PaintOpType::DrawOval: {
        const auto& op = static_cast<const DrawOvalOp&>(base_op);
        str << "DrawOvalOp(oval=" << ToString(op.oval)
            << ", flags=" << ToString(op.flags) << ")";
        break;
      }
      case PaintOpType::DrawPath: {
        const auto& op = static_cast<const DrawPathOp&>(base_op);
        str << "DrawPathOp(path=" << ToString(op.path)
            << ", flags=" << ToString(op.flags)
            << ", use_cache=" << (op.use_cache == UsePaintCache::kEnabled)
            << ")";
        break;
      }
      case PaintOpType::DrawRecord: {
        const auto& op = static_cast<const DrawRecordOp&>(base_op);
        str << "DrawRecordOp(record=" << ToString(op.record) << ")";
        break;
      }
      case PaintOpType::DrawRect: {
        const auto& op = static_cast<const DrawRectOp&>(base_op);
        str << "DrawRectOp(rect=" << ToString(op.rect)
            << ", flags=" << ToString(op.flags) << ")";
        break;
      }
      case PaintOpType::DrawRRect: {
        const auto& op = static_cast<const DrawRRectOp&>(base_op);
        str << "DrawRRectOp(rrect=" << ToString(op.rrect)
            << ", flags=" << ToString(op.flags) << ")";
        break;
      }
      case PaintOpType::DrawSkottie: {
        const auto& op = static_cast<const DrawSkottieOp&>(base_op);
        str << "DrawSkottieOp("
            << "skottie=" << ToString(op.skottie)
            << ", dst=" << ToString(op.dst) << ", t=" << op.t << ")";
        break;
      }
      case PaintOpType::DrawTextBlob: {
        const auto& op = static_cast<const DrawTextBlobOp&>(base_op);
        str << "DrawTextBlobOp(blob=" << ToString(op.blob)
            << ", x=" << ToString(op.x) << ", y=" << ToString(op.y)
            << ", flags=" << ToString(op.flags) << ")";
        break;
      }
      case PaintOpType::Noop: {
        str << "NoopOp()";
        break;
      }
      case PaintOpType::Restore: {
        str << "RestoreOp()";
        break;
      }
      case PaintOpType::Rotate: {
        const auto& op = static_cast<const RotateOp&>(base_op);
        str << "RotateOp(degrees=" << ToString(op.degrees) << ")";
        break;
      }
      case PaintOpType::Save: {
        str << "SaveOp()";
        break;
      }
      case PaintOpType::SaveLayer: {
        const auto& op = static_cast<const SaveLayerOp&>(base_op);
        str << "SaveLayerOp(bounds=" << ToString(op.bounds)
            << ", flags=" << ToString(op.flags) << ")";
        break;
      }
      case PaintOpType::SaveLayerAlpha: {
        const auto& op = static_cast<const SaveLayerAlphaOp&>(base_op);
        str << "SaveLayerAlphaOp(bounds=" << ToString(op.bounds)
            << ", alpha=" << static_cast<uint32_t>(op.alpha * 255) << ")";
        break;
      }
      case PaintOpType::Scale: {
        const auto& op = static_cast<const ScaleOp&>(base_op);
        str << "ScaleOp(sx=" << ToString(op.sx) << ", sy=" << ToString(op.sy)
            << ")";
        break;
      }
      case PaintOpType::SetMatrix: {
        const auto& op = static_cast<const SetMatrixOp&>(base_op);
        str << "SetMatrixOp(matrix=" << ToString(op.matrix) << ")";
        break;
      }
      case PaintOpType::Translate: {
        const auto& op = static_cast<const TranslateOp&>(base_op);
        str << "TranslateOp(dx=" << ToString(op.dx)
            << ", dy=" << ToString(op.dy) << ")";
        break;
      }
      case PaintOpType::SetNodeId: {
        const auto& op = static_cast<const SetNodeIdOp&>(base_op);
        str << "SetNodeIdOp(id=" << op.node_id << ")";
        break;
      }
    }
    return str.str();
  }

  static std::string ToString(const SkScalar& scalar) {
    return base::StringPrintf("%.3f", scalar);
  }

  static std::string ToString(const SkPoint& point) {
    return base::StringPrintf("[%.3f,%.3f]", point.fX, point.fY);
  }

  static std::string ToString(const SkRect& rect) {
    return base::StringPrintf("[%.3f,%.3f %.3fx%.3f]", rect.x(), rect.y(),
                              rect.width(), rect.height());
  }

  static std::string ToString(const SkIRect& rect) {
    return base::StringPrintf("[%d,%d %dx%d]", rect.x(), rect.y(), rect.width(),
                              rect.height());
  }

  static std::string ToString(const SkRRect& rect) {
    return base::StringPrintf("[bounded by %.3f,%.3f %.3fx%.3f]",
                              rect.rect().x(), rect.rect().y(),
                              rect.rect().width(), rect.rect().height());
  }

  static std::string ToString(const SkM44& matrix) {
    return base::StringPrintf(
        "[%8.4f %8.4f %8.4f %8.4f][%8.4f %8.4f %8.4f %8.4f][%8.4f %8.4f %8.4f "
        "%8.4f][%8.4f %8.4f %8.4f %8.4f]]",
        matrix.rc(0, 0), matrix.rc(0, 1), matrix.rc(0, 2), matrix.rc(0, 3),
        matrix.rc(1, 0), matrix.rc(1, 1), matrix.rc(1, 2), matrix.rc(1, 3),
        matrix.rc(2, 0), matrix.rc(2, 1), matrix.rc(2, 2), matrix.rc(2, 3),
        matrix.rc(3, 0), matrix.rc(3, 1), matrix.rc(3, 2), matrix.rc(3, 3));
  }

  static std::string ToString(const SkMatrix& matrix) {
    return base::StringPrintf(
        "[%8.4f %8.4f %8.4f][%8.4f %8.4f %8.4f][%8.4f %8.4f %8.4f]]",
        matrix.rc(0, 0), matrix.rc(0, 1), matrix.rc(0, 2), matrix.rc(1, 0),
        matrix.rc(1, 1), matrix.rc(1, 2), matrix.rc(2, 0), matrix.rc(2, 1),
        matrix.rc(2, 2));
  }

  static std::string ToString(const SkColor& color) {
    return base::StringPrintf("rgba(%d, %d, %d, %d)", SkColorGetR(color),
                              SkColorGetG(color), SkColorGetB(color),
                              SkColorGetA(color));
  }

  static std::string ToString(const SkColor4f& color) {
    return base::StringPrintf("rgba(%f, %f, %f, %f)", color.fR, color.fG,
                              color.fB, color.fA);
  }

  static std::string ToString(const SkBlendMode& mode) {
    switch (mode) {
      default:
        break;
      case SkBlendMode::kClear:
        return "kClear";
      case SkBlendMode::kSrc:
        return "kSrc";
      case SkBlendMode::kDst:
        return "kDst";
      case SkBlendMode::kSrcOver:
        return "kSrcOver";
      case SkBlendMode::kDstOver:
        return "kDstOver";
      case SkBlendMode::kSrcIn:
        return "kSrcIn";
      case SkBlendMode::kDstIn:
        return "kDstIn";
      case SkBlendMode::kSrcOut:
        return "kSrcOut";
      case SkBlendMode::kDstOut:
        return "kDstOut";
      case SkBlendMode::kSrcATop:
        return "kSrcATop";
      case SkBlendMode::kDstATop:
        return "kDstATop";
      case SkBlendMode::kXor:
        return "kXor";
      case SkBlendMode::kPlus:
        return "kPlus";
      case SkBlendMode::kModulate:
        return "kModulate";
      case SkBlendMode::kScreen:
        return "kScreen";
      case SkBlendMode::kOverlay:
        return "kOverlay";
      case SkBlendMode::kDarken:
        return "kDarken";
      case SkBlendMode::kLighten:
        return "kLighten";
      case SkBlendMode::kColorDodge:
        return "kColorDodge";
      case SkBlendMode::kColorBurn:
        return "kColorBurn";
      case SkBlendMode::kHardLight:
        return "kHardLight";
      case SkBlendMode::kSoftLight:
        return "kSoftLight";
      case SkBlendMode::kDifference:
        return "kDifference";
      case SkBlendMode::kExclusion:
        return "kExclusion";
      case SkBlendMode::kMultiply:
        return "kMultiply";
      case SkBlendMode::kHue:
        return "kHue";
      case SkBlendMode::kSaturation:
        return "kSaturation";
      case SkBlendMode::kColor:
        return "kColor";
      case SkBlendMode::kLuminosity:
        return "kLuminosity";
    }
    return "<unknown SkBlendMode>";
  }

  static std::string ToString(const SkClipOp& op) {
    switch (op) {
      default:
        break;
      case SkClipOp::kDifference:
        return "kDifference";
      case SkClipOp::kIntersect:
        return "kIntersect";
    }
    return "<unknown SkClipOp>";
  }

  static std::string ToString(const SkData& data) { return "<SkData>"; }

  static std::string ToString(const ThreadsafePath& path) {
    return ToString(static_cast<const SkPath&>(path));
  }

  static std::string ToString(const SkPath& path) {
    // TODO(vmpstr): SkPath has a dump function which we can use here?
    return "<SkPath>";
  }

  static std::string ToString(PaintFlags::FilterQuality quality) {
    switch (quality) {
      case PaintFlags::FilterQuality::kNone:
        return "kNone_SkFilterQuality";
      case PaintFlags::FilterQuality::kLow:
        return "kLow_SkFilterQuality";
      case PaintFlags::FilterQuality::kMedium:
        return "kMedium_SkFilterQuality";
      case PaintFlags::FilterQuality::kHigh:
        return "kHigh_SkFilterQuality";
    }
    return "<unknown FilterQuality>";
  }

  static std::string ToString(PaintFlags::Cap cap) {
    switch (cap) {
      case PaintFlags::kButt_Cap:
        return "kButt_Cap";
      case PaintFlags::kRound_Cap:
        return "kRound_Cap";
      case PaintFlags::kSquare_Cap:
        return "kSquare_Cap";
    }
    return "<unknown PaintFlags::Cap>";
  }

  static std::string ToString(PaintFlags::Join join) {
    switch (join) {
      case PaintFlags::kMiter_Join:
        return "kMiter_Join";
      case PaintFlags::kRound_Join:
        return "kRound_Join";
      case PaintFlags::kBevel_Join:
        return "kBevel_Join";
    }
    return "<unknown PaintFlags::Join>";
  }

  static std::string ToString(const SkColorFilter& filter) {
    return "SkColorFilter";
  }

  static std::string ToString(const SkMaskFilter& filter) {
    return "SkMaskFilter";
  }

  static std::string ToString(const SkPathEffect& effect) {
    return "SkPathEffect";
  }

  static std::string ToString(const SkDrawLooper& looper) {
    return "SkDrawLooper";
  }

  static std::string ToString(PaintCanvas::AnnotationType type) {
    switch (type) {
      default:
        break;
      case PaintCanvas::AnnotationType::URL:
        return "URL";
      case PaintCanvas::AnnotationType::NAMED_DESTINATION:
        return "NAMED_DESTINATION";
      case PaintCanvas::AnnotationType::LINK_TO_DESTINATION:
        return "LINK_TO_DESTINATION";
    }
    return "<unknown AnnotationType>";
  }

  static std::string ToString(const SkCanvas::SrcRectConstraint& constraint) {
    switch (constraint) {
      default:
        break;
      case SkCanvas::kStrict_SrcRectConstraint:
        return "kStrict_SrcRectConstraint";
      case SkCanvas::kFast_SrcRectConstraint:
        return "kFast_SrcRectConstraint";
    }
    return "<unknown SrcRectConstraint>";
  }

  static std::string ToString(PaintShader::ScalingBehavior behavior) {
    switch (behavior) {
      case PaintShader::ScalingBehavior::kRasterAtScale:
        return "kRasterAtScale";
      case PaintShader::ScalingBehavior::kFixedScale:
        return "kFixedScale";
    }
    return "<unknown ScalingBehavior>";
  }

  static std::string ToString(PaintShader::Type type) {
    switch (type) {
      case PaintShader::Type::kEmpty:
        return "kEmpty";
      case PaintShader::Type::kColor:
        return "kColor";
      case PaintShader::Type::kLinearGradient:
        return "kLinearGradient";
      case PaintShader::Type::kRadialGradient:
        return "kRadialGradient";
      case PaintShader::Type::kTwoPointConicalGradient:
        return "kTwoPointConicalGradient";
      case PaintShader::Type::kSweepGradient:
        return "kSweepGradient";
      case PaintShader::Type::kImage:
        return "kImage";
      case PaintShader::Type::kPaintRecord:
        return "kPaintRecord";
      case PaintShader::Type::kShaderCount:
        return "kShaderCount";
    }
    return "<unknown PaintShader::Type>";
  }

  static std::string ToString(const PaintImage& image) {
    return "<paint image>";
  }

  static std::string ToString(const SkottieWrapper& skottie) {
    std::ostringstream str;
    str << "<skottie [";
    str << "duration=" << skottie.duration() << " seconds";
    str << ", width=" << ToString(skottie.size().width());
    str << ", height=" << ToString(skottie.size().height());
    str << "]";
    return str.str();
  }

  static std::string ToString(const PaintRecord& record) {
    return record.empty() ? "(empty)" : "<paint record>";
  }

  static std::string ToString(const SkTextBlob& blob) {
    return "<sk text blob>";
  }

  static std::string ToString(const gfx::SizeF& size) {
    return base::StringPrintf("[%s]", size.ToString().c_str());
  }

  static std::string ToString(const PaintShader& shader) {
    std::ostringstream str;
    str << "[type=" << ToString(shader.shader_type());
    str << ", flags=" << shader.flags_;
    str << ", end_radius=" << shader.end_radius_;
    str << ", start_radius=" << shader.start_radius_;
    str << ", tx=" << static_cast<unsigned>(shader.tx_);
    str << ", ty=" << static_cast<unsigned>(shader.ty_);
    str << ", fallback_color=" << ToString(shader.fallback_color_);
    str << ", scaling_behavior=" << ToString(shader.scaling_behavior_);
    str << ", local_matrix=" << ToString(shader.local_matrix_);
    str << ", center=" << ToString(shader.center_);
    str << ", tile=" << ToString(shader.tile_);
    str << ", start_point=" << ToString(shader.start_point_);
    str << ", end_point=" << ToString(shader.end_point_);
    str << ", start_degrees=" << shader.start_degrees_;
    str << ", end_degrees=" << shader.end_degrees_;
    if (shader.shader_type() == PaintShader::Type::kImage) {
      str << ", image=" << ToString(shader.image_);
    } else {
      str << ", image=(nil)";
    }
    str << ", record=" << ToString(shader.record_);
    str << ", id=" << shader.id_;
    str << ", tile_scale=" << ToString(shader.tile_scale_);
    if (shader.colors_.size() > 0) {
      str << ", colors=[" << ToString(shader.colors_[0]);
      for (size_t i = 1; i < shader.colors_.size(); ++i) {
        str << ", " << ToString(shader.colors_[i]);
      }
      str << "]";
    } else {
      str << ", colors=(nil)";
    }
    if (shader.positions_.size() > 0) {
      str << ", positions=[" << shader.positions_[0];
      for (size_t i = 1; i < shader.positions_.size(); ++i) {
        str << ", " << shader.positions_[i];
      }
      str << "]";
    } else {
      str << ", positions=(nil)";
    }
    str << "]";

    return str.str();
  }

  static std::string ToString(const PaintFilter& filter) {
    return "<PaintFilter>";
  }

  static std::string ToString(const PaintFlags& flags) {
    std::ostringstream str;
    str << std::boolalpha;
    str << "[color=" << ToString(flags.getColor());
    str << ", blendMode=" << ToString(flags.getBlendMode());
    str << ", isAntiAlias=" << flags.isAntiAlias();
    str << ", isDither=" << flags.isDither();
    str << ", filterQuality=" << ToString(flags.getFilterQuality());
    str << ", strokeWidth=" << ToString(flags.getStrokeWidth());
    str << ", strokeMiter=" << ToString(flags.getStrokeMiter());
    str << ", strokeCap=" << ToString(flags.getStrokeCap());
    str << ", strokeJoin=" << ToString(flags.getStrokeJoin());
    str << ", colorFilter=" << ToString(flags.getColorFilter());
    str << ", maskFilter=" << ToString(flags.getMaskFilter());
    str << ", shader=" << ToString(flags.getShader());
    str << ", hasShader=" << flags.HasShader();
    str << ", shaderIsOpaque=" << (flags.HasShader() && flags.ShaderIsOpaque());
    str << ", pathEffect=" << ToString(flags.getPathEffect());
    str << ", imageFilter=" << ToString(flags.getImageFilter());
    str << ", drawLooper=" << ToString(flags.getLooper());
    str << ", supportsFoldingAlpha=" << flags.SupportsFoldingAlpha();
    str << ", isValid=" << flags.IsValid();
    str << ", hasDiscardableImages=" << flags.HasDiscardableImages();
    str << "]";
    return str.str();
  }
};

}  // namespace cc

#endif  // CC_TEST_PAINT_OP_HELPER_H_
