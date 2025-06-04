from flask import Flask, request, jsonify
import time
import tensorflow as tf
import torch
from transformers import pipeline

app = Flask(__name__)

# Global variable for QA pipeline.
qa_pipeline = None

@app.route('/load_model_bert', methods=['POST'])
def load_model_bert():
    global qa_pipeline
    try:
        qa_pipeline = pipeline(
            "question-answering",
            model="csarron/mobilebert-uncased-squad-v2",
            tokenizer="csarron/mobilebert-uncased-squad-v2"
        )
        app.logger.info("Model loaded successfully.")
        return jsonify({"status": "Model loaded successfully."})
    except Exception as e:
        app.logger.error("Failed to load model", exc_info=True)
        return jsonify({"error": f"Failed to load model: {str(e)}"}), 500

@app.route('/infer_single_bert', methods=['POST'])
def infer_single_bert():
    global qa_pipeline
    if qa_pipeline is None:
        return jsonify({"error": "Model not loaded. Please call /load_model_bert first."}), 400
    data = request.get_json()
    if not data or 'question' not in data or 'context' not in data:
        return jsonify({"error": "Payload must contain 'question' and 'context' fields."}), 400
    try:
        prediction = qa_pipeline({
            'question': data['question'],
            'context': data['context']
        })
        return jsonify(prediction)
    except Exception as e:
        app.logger.error("Inference error", exc_info=True)
        return jsonify({"error": f"Inference error: {str(e)}"}), 500

@app.route('/infer_batch_bert', methods=['POST'])
def infer_batch_bert():
    global qa_pipeline
    if qa_pipeline is None:
        return jsonify({"error": "Model not loaded. Please call /load_model_bert first."}), 400
    data = request.get_json()
    if not data or not isinstance(data, list):
        return jsonify({"error": "Payload must be an array of objects with 'question' and 'context' fields."}), 400
    results = []
    for idx, item in enumerate(data):
        if 'question' not in item or 'context' not in item:
            results.append({"error": f"Item at index {idx} is missing 'question' or 'context'."})
        else:
            try:
                prediction = qa_pipeline({
                    'question': item['question'],
                    'context': item['context']
                })
                results.append(prediction)
            except Exception as e:
                results.append({"error": f"Inference error at index {idx}: {str(e)}"})
    return jsonify(results)
@app.route('/train_model', methods=['POST'])
def train_model():
    try:
        # Load MNIST data.
        (x_train, y_train), _ = tf.keras.datasets.mnist.load_data()
        x_train = x_train.astype('float32') / 255.0  # normalize

        # Convert labels to one-hot encoding.
        y_train_onehot = tf.keras.utils.to_categorical(y_train, 10)

        # Mimic tfjs setup by taking a subset.
        TRAIN_DATA_SIZE = 5500
        x_train = x_train[:TRAIN_DATA_SIZE]
        y_train_onehot = y_train_onehot[:TRAIN_DATA_SIZE]

        # Define a simple model: Flatten -> Dense(128, relu) -> Dense(10, softmax)
        model = tf.keras.Sequential([
            tf.keras.layers.Flatten(input_shape=(28, 28)),
            tf.keras.layers.Dense(128, activation='relu'),
            tf.keras.layers.Dense(10, activation='softmax')
        ])
        model.compile(
            optimizer='adam',
            loss='categorical_crossentropy',  # Corrected loss string
            metrics=['accuracy']
        )

        # Start timing the training.
        start_time = time.perf_counter()
        history = model.fit(x_train, y_train_onehot, epochs=10, batch_size=512, verbose=0)
        end_time = time.perf_counter()

        training_time_ms = (end_time - start_time) * 1000  # milliseconds
        accuracy = history.history['accuracy'][-1]

        result = {
            "training_time_ms": training_time_ms,
            "accuracy": accuracy
        }
        return jsonify(result)
    except Exception as e:
        return jsonify({"error": f"Training error: {str(e)}"}), 500

@app.route("/test", methods=["GET"])
def analyze():
    return jsonify({"message": "OP server is working"})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
